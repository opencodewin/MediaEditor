#ifndef DLCLASS_H
#define DLCLASS_H

#include <memory>
#include <string>
#if defined(_MSC_VER) || defined(_WIN32)
#include "dlfcn_win.h"
#else
#include <dlfcn.h>
#endif
#include <iostream>

template <class T>
class DLClass {

public:
	DLClass(std::string module_name);
	~DLClass();

	std::shared_ptr<T> make_obj();
	int32_t get_version();
	int32_t get_api_version();
	std::string get_module_path();

private:
	struct shared_obj {
		typename T::version_t* version = NULL;
		typename T::version_t* api_version = NULL;
		typename T::create_t* create = NULL;
		typename T::destroy_t* destroy = NULL;
		void* dll_handle = NULL;

		~shared_obj();
		bool open_module(std::string module);
		void close_module();
		std::string module_path;
	};

	std::string module;
	std::shared_ptr<shared_obj> shared;
};

template <class T>
DLClass<T>::DLClass(std::string module_name) :
	module(module_name) {
	shared = std::make_shared<shared_obj>();
}

template <class T>
DLClass<T>::~DLClass() {
    if (shared)
        shared->close_module();
}

template <class T>
void DLClass<T>::shared_obj::close_module() {
	if (dll_handle) {
		dlclose(dll_handle);
		dll_handle = NULL;
	}
	if (version) version = NULL;
	if (api_version) api_version = NULL;
	if (create) create = NULL;
	if (destroy) destroy = NULL;
}

template <class T>
bool DLClass<T>::shared_obj::open_module(std::string module) {

	dll_handle = dlopen(module.c_str(), RTLD_LAZY);

	if (!dll_handle) {
		std::cerr << "Failed to open library: " << dlerror() << std::endl;
		return false;
	}

	// Reset errors
	dlerror();

	version = (typename T::version_t*) dlsym(dll_handle, "version");
	const char* err = dlerror();
	if (err) {
		std::cerr << "Failed to load version symbol: " << err << std::endl;
		close_module();
		return false;
	}

	api_version = (typename T::version_t*) dlsym(dll_handle, "api_version");
	err = dlerror();
	if (err) {
		std::cerr << "Failed to load api version symbol: " << err << std::endl;
		close_module();
		return false;
	}

	create = (typename T::create_t*) dlsym(dll_handle, "create");
	err = dlerror();
	if (err) {
		std::cerr << "Failed to load create symbol: " << err << std::endl;
		close_module();
		return false;
	}

	destroy = (typename T::destroy_t*) dlsym(dll_handle, "destroy");
	err = dlerror();
	if (err) {
		std::cerr << "Failed to load destroy symbol: " << err << std::endl;
		close_module();
		return false;
	}

	Dl_info info;
	dladdr((void *)version, &info);
	module_path = std::string(info.dli_fname);
	return true;
}

template <class T>
int32_t DLClass<T>::get_version() {
	return shared->version();
}

template <class T>
int32_t DLClass<T>::get_api_version() {
	return shared->api_version();
}

template <class T>
std::string DLClass<T>::get_module_path() {
	return shared->module_path;
}

template <class T>
std::shared_ptr<T> DLClass<T>::make_obj() {
	if (!shared->create || !shared->destroy) {
		if (!shared->open_module(module)) {
			return std::shared_ptr<T>(NULL);
		}
	}

	//    auto create_args = ((T* (*)(Args...))create);    
	std::shared_ptr<shared_obj> my_shared = shared;
	auto the_ptr = shared->create();
	if (the_ptr == nullptr || the_ptr == NULL) {
		std::cerr << "Failed to load the dynamic obj: obj->create returns nullptr or NULL." << std::endl;
		return nullptr;
	}
	return std::shared_ptr<T>(the_ptr,
		[my_shared](T* p) { my_shared->destroy(p); });
}

template <class T>
DLClass<T>::shared_obj::~shared_obj() {
	close_module();
}

#endif
