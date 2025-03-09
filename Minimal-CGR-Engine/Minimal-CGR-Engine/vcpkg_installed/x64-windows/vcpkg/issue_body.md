Package: zlib:x64-windows@1.2.8

**Host Environment**

- Host: x64-windows
- Compiler: MSVC 19.42.34436.0
-    vcpkg-tool version: 2024-09-30-ab8988503c7cffabfd440b243a383c0a352a023d
    vcpkg-readonly: true
    vcpkg-scripts version: 2960d7d80e8d09c84ae8abf15c12196c2ca7d39a

**To Reproduce**

`vcpkg install `

**Failure logs**

```
CMake Warning at scripts/cmake/vcpkg_common_functions.cmake:3 (message):
  vcpkg_common_functions has been removed and all values are automatically
  provided in all portfile.cmake invocations.  Please remove
  `include(vcpkg_common_functions)`.
Call Stack (most recent call first):
  C:/Users/Marvi/AppData/Local/vcpkg/registries/git-trees/3309ec82cd96d752ff890c441cb20ef49b52bf94/portfile.cmake:1 (include)
  scripts/ports.cmake:192 (include)


-- Downloading http://zlib.net/zlib128.zip -> zlib128.zip...
[DEBUG] To include the environment variables in debug output, pass --debug-env
[DEBUG] Trying to load bundleconfig from C:\Program Files\Microsoft Visual Studio\2022\Community\VC\vcpkg\vcpkg-bundle.json
[DEBUG] Bundle config: readonly=true, usegitregistry=true, embeddedsha=2960d7d80e8d09c84ae8abf15c12196c2ca7d39a, deployment=VisualStudio, vsversion=17.0
[DEBUG] VS telemetry opted in at SOFTWARE\WOW6432Node\Microsoft\VSCommon\17.0\SQM\\OptIn
[DEBUG] Metrics enabled.
[DEBUG] Feature flag 'binarycaching' unset
[DEBUG] Feature flag 'compilertracking' unset
[DEBUG] Feature flag 'registries' unset
[DEBUG] Feature flag 'versions' unset
[DEBUG] Feature flag 'dependencygraph' unset
error: Missing zlib128.zip and downloads are blocked by x-block-origin.
error: http://zlib.net/zlib128.zip: failed: status code 404
[DEBUG] D:\a\_work\1\s\src\vcpkg\base\downloads.cpp(1030): 
[DEBUG] Time in subprocesses: 0us
[DEBUG] Time in parsing JSON: 14us
[DEBUG] Time in JSON reader: 0us
[DEBUG] Time in filesystem: 1360us
[DEBUG] Time in loading ports: 0us
[DEBUG] Exiting after 445 ms (432142us)

CMake Error at scripts/cmake/vcpkg_download_distfile.cmake:32 (message):
      
      Failed to download file with error: 1
      If you are using a proxy, please check your proxy setting. Possible causes are:
      
      1. You are actually using an HTTP proxy, but setting HTTPS_PROXY variable
         to `https://address:port`. This is not correct, because `https://` prefix
         claims the proxy is an HTTPS proxy, while your proxy (v2ray, shadowsocksr
         , etc..) is an HTTP proxy. Try setting `http://address:port` to both
         HTTP_PROXY and HTTPS_PROXY instead.
      
      2. If you are using Windows, vcpkg will automatically use your Windows IE Proxy Settings
         set by your proxy software. See https://github.com/microsoft/vcpkg-tool/pull/77
         The value set by your proxy might be wrong, or have same `https://` prefix issue.
      
      3. Your proxy's remote server is out of service.
      
      If you've tried directly download the link, and believe this is not a temporary
      download server failure, please submit an issue at https://github.com/Microsoft/vcpkg/issues
      to report this upstream download server failure.
      

Call Stack (most recent call first):
  scripts/cmake/vcpkg_download_distfile.cmake:270 (z_vcpkg_download_distfile_show_proxy_and_fail)
  C:/Users/Marvi/AppData/Local/vcpkg/registries/git-trees/3309ec82cd96d752ff890c441cb20ef49b52bf94/portfile.cmake:3 (vcpkg_download_distfile)
  scripts/ports.cmake:192 (include)



```

**Additional context**

<details><summary>vcpkg.json</summary>

```
{
  "dependencies": [
    "fmt",
    "zlib"
  ],
  "builtin-baseline": "3426db05b996481ca31e95fff3734cf23e0f51bc",
  "overrides": [
    {
      "name": "zlib",
      "version": "1.2.8"
    }
  ]
}

```
</details>
