#include "imgui_fft.h"
#include <cmath>

namespace ImGui
{
// FFT 1D
inline void swap(float* a, float* b)
{
	float t = *a;
	*a = *b;
	*b = t;
}

inline float sqr(float arg)
{
	return arg * arg;
}

void ImFFT(float* data, int N,  bool forward)
{
	int n = N << 1;
	int i, j, m, mmax;

	/* bit reversal section */

	j = 0;
	for (i = 0; i < n; i += 2) 
	{
		if (j > i) 
		{
			swap (&data[j], &data[i]);
			swap (&data[j + 1], &data[i + 1]);
		}
		m = N;
		while (m >= 2 && j >= m) 
		{
			j -= m;
			m >>= 1;
		}
		j += m;
	}

	/* Daniel-Lanczos section */

	float theta, wr, wpr, wpi, wi, wtemp;
	float tempr, tempi;
	for (mmax = 2; n > mmax;) 
	{
		int istep = mmax << 1;
		theta = (forward ? 1 : -1) * (2.0 * M_PI / mmax);
		wpr = -2.0 * sqr (sin (0.5 * theta));
		wpi = sin (theta);
		wr = 1.0;
		wi = 0.0;
		for (m = 0; m < mmax; m += 2) 
		{
			for (i = m; i < n; i += istep) 
			{
				j = i + mmax;
				tempr = wr * data[j] - wi * data[j + 1];
				tempi = wr * data[j + 1] + wi * data[j];
				data[j] = data[i] - tempr;
				data[j + 1] = data[i + 1] - tempi;
				data[i] += tempr;
				data[i + 1] += tempi;
			}
			wr = (wtemp = wr) * wpr - wi * wpi + wr;
			wi = wi * wpr + wtemp * wpi + wi;
		}
		mmax = istep;
	}

	/* normalisation section */
	const float tmp = 1.0 / sqrt ((float)N);
	for (i = 0; i < n; i++)
		data[i] *= tmp;
}

void ImRFFT(float* data, int N,  bool forward)
{
	/* main section */
	int k;
	float c1 = 0.5, c2;
	float theta = M_PI / (float) (N >> 1);

	if (forward) 
	{
		c2 = -0.5;
		ImFFT(data, N >> 1, forward);
	} 
	else 
	{
		c2 = +0.5;
		theta = -theta;
	}

	float wpr = -2.0 * sqr (sin (0.5 * theta));
	float wpi = sin (theta);
	float wr = 1.0 + wpr;
	float wi = wpi;
	float wtemp;

	int i, i1, i2, i3, i4;
	float h1r, h1i, h2r, h2i;
	for (i = 1; i < N >> 2; i++) 
	{
		i1 = i + i;
		i2 = i1 + 1;
		i3 = N - i1;
		i4 = i3 + 1;
		h1r = c1 * (data[i1] + data[i3]);
		h1i = c1 * (data[i2] - data[i4]);
		h2r = -c2 * (data[i2] + data[i4]);
		h2i = c2 * (data[i1] - data[i3]);
		data[i1] = h1r + wr * h2r - wi * h2i;
		data[i2] = h1i + wr * h2i + wi * h2r;
		data[i3] = h1r - wr * h2r + wi * h2i;
		data[i4] = -h1i + wr * h2i + wi * h2r;
		wr = (wtemp = wr) * wpr - wi * wpi + wr;
		wi = wi * wpr + wtemp * wpi + wi;
	}

	if (forward) 
	{
		data[0] = (h1r = data[0]) + data[1];
		data[1] = h1r - data[1];
	}
	else 
	{
		data[0] = c1 * ((h1r = data[0]) + data[1]);
		data[1] = c1 * (h1r - data[1]);
		ImFFT(data, N >> 1, forward);
	}

	/* normalisation section */
	//const float tmp = forward ? M_SQRT1_2 : M_SQRT2;
	//for (k = 0; k < N; k++)
	//	data[k] *= tmp;
}

void ImRFFT(float* in, float* out, int N,  bool forward)
{
    memcpy(out, in, sizeof(float) * N);
    ImRFFT(out, N, forward);
}

int ImReComposeDB(float * in, float * out, int samples, bool inverse)
{
	int i,max = 0;
	float zero_db,db,max_db = -FLT_MAX;
	float amplitude;
    int N = samples >> 1;
	zero_db = - 20 * log10((float)(1<<15));
	for (i = 0; i < N + 1; i++)
	{
		if (i != 0/* && i != N*/)
		{
			amplitude = sqrt(sqr(in[2 * i]) + sqr(in[2 * i + 1]));
		}
		else
		{
			amplitude = 1.0f / (1<<15);
		}
		db = 20 * log10(amplitude) - (inverse ? zero_db : 0);
		out[i] = db;
		if (db > max_db)
		{
			max_db = db;
			max = i;
		}
	}
	return max;
}

int ImReComposeAmplitude(float * in, float * out, int samples)
{
	int i,max = 0;
	float tmp,dmax = 0;
	for (i = 0; i < (samples >> 1) + 1; i++)
	{
		if (i != 0/* && i != (samples >> 1)*/)
		{
			tmp = sqrt(sqr(in[2 * i]) + sqr(in[2 * i + 1]));
		}
		else
		{
			tmp = 0;//sqr(in[i == 0 ? 0 : 1]);
		}
		out[i] = tmp;
		if (tmp > dmax)
		{
			dmax = tmp;
			max = i;
		}
	}
	return max;
}

int ImReComposePhase(float * in, float * out, int samples)
{
    for (int i = 0; i < (samples >> 1) + 1; i++)
	{
        float hAngle = 0;
        float dx = in[2 * i];
        float dy = in[2 * i + 1];
        hAngle = atan2(dy, dx);
        hAngle = 180.f * hAngle / M_PI;
        out[i] = hAngle;
    }
	return 0;
}

int ImReComposeDBShort(float * in, float * out, int samples, bool inverse)
{
	int i,j;
	int n_sample;
	int start_sample;
	float zero_db;
	float tmp;
	static unsigned int freq_table[21] = {0,1,2,3,4,5,6,7,8,11,15,20,
		27,36,47,62,82,107,141,184,255}; //fft_number

	zero_db = - 20 * log10((float)(1<<15));

	for (i = 0; i< 20; i++)
	{
		start_sample = freq_table[i] * (samples / 256);
		n_sample = (freq_table[i + 1] - freq_table[i])  * (samples / 256);
		tmp=0;
		for (j = start_sample; j < start_sample + n_sample; j++)
		{
			tmp += 2 * sqr(in[j]);
		}
		
		tmp /= (float)n_sample;
		out[i] = 20.0 * log10(tmp) - (inverse ? zero_db : 0);
	}
	return 20;
}

int ImReComposeDBLong(float * in, float * out, int samples, bool inverse)
{
	int i,j;
	int n_sample;
	int start_sample;
	float zero_db;
	float tmp;
	static unsigned int freq_table[77] = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,
		19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,
		35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,
		52,53,54,55,56,57,58,61,66,71,76,81,87,93,100,107,
		114,122,131,140,150,161,172,184,255}; //fft_number

	zero_db = - 20 * log10((float)(1<<15));

	for (i = 0; i< 76; i++)
	{
		start_sample = freq_table[i] * (samples / 256);
		n_sample = (freq_table[i + 1] - freq_table[i]) * (samples / 256);
		tmp=0;
		for (j = start_sample; j < start_sample + n_sample; j++)
		{
			tmp += 2 * sqr(in[j]);
		}
		
		tmp /= (float)n_sample;
		out[i] = 20.0 * log10(tmp) - (inverse ? zero_db : 0);
	}
	return 76;
}

float ImDoDecibel(float * in, int samples, bool inverse)
{
	int i;
	float db,zero_db;
	float tmp;
	zero_db = - 20 * log10((float)(1<<15));
	tmp = 0;
	for (i = 0; i < (samples >> 1) + 1; i++)
	{
		if (i != 0 && i != (samples >> 1))
		{
			tmp += 2 * (sqr(in[2 * i]) + sqr(in[2 * i+1]));
		}
		else
		{
			tmp += sqr(in[i == 0 ? 0 : 1]);
		}
	}
	tmp /= (float)samples;
	db = 20 * log10(tmp) - (inverse ? zero_db : 0);
	return db;
}

struct HannWindow
{
    inline HannWindow(int _frame_size, int _shift_size)
    {
        float tmp = 0;
        shift_size = _shift_size;
        frame_size = _frame_size;
        hann = new float[frame_size];
        for (int i = 0; i < frame_size; i++) hann[i] = 0.5 * (1.0 - cos(2.0 * M_PI * (i / (float)frame_size)));
        for (int i = 0; i < frame_size; i++) tmp += hann[i] * hann[i];
        tmp /= shift_size;
        tmp = std::sqrt(tmp);
        for (int i = 0; i < frame_size; i++) hann[i] /= tmp;
    }
    inline ~HannWindow() { delete[] hann; };
    inline void Process(float * buffer) { for (int i = 0; i < frame_size; i++) buffer[i] *= hann[i]; }

private:
    float *hann {nullptr};
    int shift_size;
    int frame_size;
};

struct WindowOverlap
{
public:
    inline WindowOverlap(uint32_t _frame_size, uint32_t _shift_size)
    {
        frame_size = _frame_size;
        shift_size = _shift_size;
        buf_offset = 0;
        num_block = frame_size / shift_size;
        output = new float[shift_size];
        buf = new float[frame_size];
        memset(buf, 0, frame_size * sizeof(float));
    }
    inline ~WindowOverlap() { delete[] buf; delete[] output; };
    inline float *overlap(float *in)
    {
        // Shift
        for (int i = 0; i < static_cast<int>(frame_size - shift_size); i++) buf[i] = buf[i + shift_size];
        // Emptying Last Block
        memset(buf + shift_size * (num_block - 1), 0, sizeof(float) * shift_size);
        // Sum
        for (int i = 0; i < static_cast<int>(frame_size); i++) buf[i] += in[i];
        // Distribution for float format
        for (int i = 0; i < static_cast<int>(shift_size); i++) output[i] = static_cast<float>(buf[i]);
        return output;
    }

private:
    uint32_t frame_size;
    uint32_t shift_size;
    uint32_t num_block;
    uint32_t buf_offset;
    float *output;
    float *buf;
};

ImSTFT::ImSTFT(int frame_, int shift_)
{
    frame_size = frame_;
    shift_size = shift_;
    overlap_size = frame_size - shift_size;
    hannwin = new HannWindow(frame_size, shift_size);
    overlap = new WindowOverlap(frame_size, shift_size);
    buf = new float[frame_size];
    memset(buf, 0, sizeof(float) * frame_size);
}

ImSTFT::~ImSTFT()
{ 
    delete (HannWindow*)hannwin; 
    delete (WindowOverlap*)overlap; 
    delete[] buf;
};

void ImSTFT::stft(float* in, float* out)
{
    /*** Shfit & Copy***/
    for (int i = 0; i < overlap_size; i++) buf[i] = buf[i + shift_size];
    for (int i = 0; i < shift_size; i++) buf[overlap_size + i] = static_cast<float>(in[i]);
    memcpy(out, buf, sizeof(float) * frame_size);
    /*** Window ***/
    ((HannWindow*)hannwin)->Process(out);
    /*** FFT ***/
    ImGui::ImRFFT(out, frame_size, true);
}

void ImSTFT::istft(float* in, float* out)
{
    /*** iFFT ***/
    ImGui::ImRFFT(in, frame_size, false);
    /*** Window ***/
    ((HannWindow*)hannwin)->Process(in);
    /*** Output ***/
    memcpy(out, ((WindowOverlap*)overlap)->overlap(in), sizeof(float) * shift_size);
}
} // namespace ImGui
