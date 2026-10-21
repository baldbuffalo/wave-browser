[![Publish Docker Image](https://github.com/wiiu-env/libcurlwrapper/actions/workflows/push_image.yml/badge.svg)](https://github.com/wiiu-env/libcurlwrapper/actions/workflows/push_image.yml)

# libcurlwrapper

This library loads CURL from an Aroma Module instead of statically linking it to the binary. This results in a much smaller binary size (saves about 750KiB).

- Requires the [CURLWrapperModule](https://github.com/wiiu-env/CURLWrapperModule) to be running via [WUMSLoader](https://github.com/wiiu-env/WUMSLoader).
- Requires [wut](https://github.com/devkitPro/wut) for building.
- Use with curl 8.7.1 headers, other versions might not be compatible.
- See important changes in the Usage section.

Install via `make install`.

## Usage

Make sure to define this in your Makefile:

```
WUMS_ROOT := $(DEVKITPRO)/wums
```

Use the normal curl headers, but link against `-lcurlwrapper` instead. For example replace `-lcurl -lmbedtls -lmbedx509 -lmbedcrypto -lz` with `-lcurlwrapper`.

**If the CURLWrapperModule is not loaded `curl_global_init` will return a negative value.**

## Changes compared to normal CURL

libcurlwrapper tries to be a drop-in replacement for libcurl, but some things need to be considered.

### All function are only available between calling `curl_global_init` and `curl_global_cleanup`.

Even functions like `curl_version` only work after calling `curl_global_init`.

### Calling `curl_easy_init` will automatically add CA certificates.

It `curl_easy_init` returns success, `curl_easy_setopt(curl, CURLOPT_CAINFO_BLOB, &blob);` with current CA certificates will be called.
These CA certificates are coming from the modules and can be updated in the future.

### CURLOPT_SOCKOPTFUNCTION

- The `CURLOPT_SOCKOPTFUNCTION` callback requires a workaround to work properly.
- This is caused by `setsockopt` not being compatible with the sockets from the CURLWrapperModule.

Currently, there are two options to get the correct `setsockopt` function:

#### Option 1: CURLOPT_SOCKOPTDATA

- To get a pointer to the correct `setsockopt` function, you have to set the `CURLOPT_SOCKOPTDATA` to 0x13371337.
- The `0x13371337` will be replaced with a pointer to the correct `setsockopt` function.

Example:

```C  
static int initSocket(void *ptr, curl_socket_t socket, curlsocktype type) {    
  int r;
  // If ptr is not our magic value, it got replaced with a pointer to a valid setsockopt function.
  if ((uint32_t) ptr != 0x13371337) {
      r = reinterpret_cast<decltype(&setsockopt)>(ptr)(socket, SOL_SOCKET, SO_WINSCALE, &o, sizeof(o));
  } else {
      r = setsockopt(socket, SOL_SOCKET, SO_WINSCALE, &o, sizeof(o));
  }
   return r == 0 ? CURL_SOCKOPT_OK : CURL_SOCKOPT_ERROR;
}
[...]
// Set socktopt data to a magic value to get a pointer to the correct setsockopt function.
curl_easy_setopt(curl, CURLOPT_SOCKOPTDATA, 0x13371337);
curl_easy_setopt(curl, CURLOPT_SOCKOPTFUNCTION, initSocket);
```

Downside of this option: the data pointer of `CURLOPT_SOCKOPTFUNCTION` can not be used.

#### Option 2: curlwrapper_setsockopt

This lib exposes the function `curlwrapper_setsockopt` which will call the correct function. Because this lib had no headers, you have to
declare it in your app like this:

````C
int curlwrapper_setsockopt(int sockfd, int level, int optname, const void *optval, socklen_t optlen);
````

When using C++, you have to add `extern "C"` at the start.

Downside of this option: Linking to the original libcurl fails because `curlwrapper_setsockopt` is only defined in this wrapper lib.

## Use this lib in Dockerfiles.

A prebuilt version of this lib can found on dockerhub. To use it for your projects, add this to your Dockerfile.

```
[...]
COPY --from=ghcr.io/wiiu-env/libcurlwrapper:[tag] /artifacts $DEVKITPRO
[...]
```

Replace [tag] with a tag you want to use, a list of tags can be found [here](https://github.com/wiiu-env/libcurlwrapper/pkgs/container/libcurlwrapper/versions).
It's highly recommended to pin the version to the **latest date** instead of using `latest`.

## Format the code via docker

`docker run --rm -v ${PWD}:/src ghcr.io/wiiu-env/clang-format:13.0.0-2 -r ./source -i`
