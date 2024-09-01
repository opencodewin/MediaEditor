#include <SDL.h>
#include <SDL_image.h>

void writePixels( FILE* f, char* _symbolNameBase, SDL_Surface* image, int* _pTotalOutputSize, bool has_alpha = false, bool is_indexed = false)
{
    fprintf( f, "extern const int %s_width;\n", _symbolNameBase );
    fprintf( f, "extern const int %s_height;\n", _symbolNameBase );
	fprintf( f, "extern const int %s_depth;\n", _symbolNameBase );
    fprintf( f, "extern const unsigned char %s_pixels[];\n\n", _symbolNameBase );
	if (is_indexed)
	{
		fprintf( f, "extern const int %s_colors;\n", _symbolNameBase );
		fprintf( f, "extern const SDL_Color %s_palette[];\n\n", _symbolNameBase );
	}

    fprintf( f, "const int %s_width = %d;\n", _symbolNameBase, image->w );
    fprintf( f, "const int %s_height = %d;\n", _symbolNameBase, image->h );
	fprintf( f, "const int %s_depth = %d;\n", _symbolNameBase, (image->pitch / image->w) * 8);
	fprintf( f, "const unsigned char %s_pixels[] =\n{\n", _symbolNameBase );

	unsigned char* pixels = (unsigned char*)image->pixels;
	int x, y;
    unsigned char b, g, r, a;
	for( y = 0; y < image->h; y++ )
	{
		fprintf( f, "\t" );
		for( x = 0; x < image->w; x++ )
		{
			int rofs = ((y * image->w) + x) * (is_indexed ? 1 : has_alpha ? 4 : 3);
			b = pixels[ rofs + 0 ];
			if (!is_indexed) g = pixels[ rofs + 1 ];
			if (!is_indexed) r = pixels[ rofs + 2 ];

			if (!is_indexed && has_alpha)
            {
                a = pixels[ rofs + 3 ];
                //fprintf( f, "0x%02x,0x%02x,0x%02x,0x%02x,", a, b, g, r );
				fprintf( f, "0x%02x,0x%02x,0x%02x,0x%02x,", b, g, r, a ); // for texture display order
            }
            else if (!is_indexed)
            {
                fprintf( f, "0x%02x,0x%02x,0x%02x,", b, g, r );
            }
			else
			{
				fprintf( f, "0x%02x,", b );
			}
		}
		fprintf( f, "\n" );
	}
	
	*_pTotalOutputSize += image->w * image->h * (is_indexed ? 1 : has_alpha ? 4 : 3);
	
	fprintf( f, "};\n\n" );

	if (is_indexed)
	{
		fprintf( f, "const int %s_colors = %d;\n", _symbolNameBase, image->format->palette->ncolors );
		fprintf( f, "const SDL_Color %s_palette[] =\n{\n", _symbolNameBase );
		for ( int i = 0; i < image->format->palette->ncolors; i++)
		{
			SDL_Color color = image->format->palette->colors[i]; 
			fprintf( f, "\t{0x%02x,0x%02x,0x%02x,0x%02x},", color.r, color.g, color.b, color.a );
			if ((i + 1)%4 == 0)
				fprintf( f, "\n" );
		}
		*_pTotalOutputSize += image->format->palette->ncolors * 4;
		fprintf( f, "};\n\n" );
	}
}


SDL_Surface* LoadImage( char* _fileName )
{
	SDL_Surface* image = IMG_Load( _fileName );
	//printf("Image=0x%016llx\n", (long long)image );
	return image;
}

FILE* openOutfileC( char* _baseOutFileName )
{
	char outname_c[ 2048 ];
	snprintf( outname_c, 2048, "%s.cpp", _baseOutFileName ); // modify by dicky reduce warning
	FILE* f = fopen( outname_c, "w" );
	
	return f;
}

int main( int _numargs, char** _apszArgh )
{
	if( _numargs != 4 )
	{
		printf("Usage error: Program need 3 arguments:\n");
		printf("  img2c <in_file.png> <out_file_base> <symbol_name>\n");
		return -1;
	}

	char* pszInFileName = _apszArgh[ 1 ];
	char* pszOutFilenameBase = _apszArgh[ 2 ];
	char* pszSymbolNameBase = _apszArgh[ 3 ];
	
	//
	SDL_Surface* image = LoadImage( pszInFileName );
	bool isAlpha = SDL_ISPIXELFORMAT_ALPHA( image->format->format );
	bool isIndexed = SDL_ISPIXELFORMAT_INDEXED( image->format->format );
	//
	// Write cpp file
	//
	FILE* f = openOutfileC( pszOutFilenameBase );

	int totalOutputSize = 0;
	writePixels( f, pszSymbolNameBase, image, &totalOutputSize, isAlpha, isIndexed);
	fclose( f );

	printf("Total output size: %i\n", totalOutputSize );
	return 0;
}
