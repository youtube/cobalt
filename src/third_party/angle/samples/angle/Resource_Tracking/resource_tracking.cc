// Note that this sample is incomplete in that it gives a snippet of code
// that demonstrates a use case of eglSetResourceTrackerANGLE(), but does not
// have any driving main() method.

#include <assert.h>

#include <iostream>
#include <sstream>
#include <string>

#include <d3d11.h>
#include <EGL/eglext.h>
#include <wrl.h>

EGLDisplay g_display;

bool GetDebugName(ID3D11Resource* resource, std::string* name) {
  char data[512];
  UINT data_size = 512;
  if (SUCCEEDED(resource->GetPrivateData(WKPDID_D3DDebugObjectName, &data_size,
                                         data))) {
    name->assign(data, data_size);
    return true;
  }

  return false;
}

void GetResourceData(const EGLResourceTrackerDataAngle *data,
                     std::stringstream *stream) {
  const char* op = (data->iOpType == EGL_ANGLE_TRACK_OPTYPE_CREATE) ? "+" : "-";
  (*stream) << " " << op << (data->iSize >> 10) << "KB;";

  if (data->iOpType == EGL_ANGLE_TRACK_OPTYPE_DESTROY) {
    // data->pResource should be treated as invalid during resource
    // deallocation events
    return;
  }

  ComPtr<IUnknown> unk(reinterpret_cast<IUnknown*>(data->pResource));

  switch (data->iType) {
    case EGL_ANGLE_TRACK_TYPE_TEX2D: {
      ComPtr<ID3D11Texture2D> tex;
      assert(SUCCEEDED(unk.As(&tex)));

      D3D11_TEXTURE2D_DESC desc;
      tex->GetDesc(&desc);

      (*stream) << " Tex2D: " << desc.Width << "x" << desc.Height << ";";

      std::string name;
      if (GetDebugName(tex.Get(), &name))
        (*stream) << " " << name << ";";

      break;
    }
    case EGL_ANGLE_TRACK_TYPE_BUFFER: {
      ComPtr<ID3D11Buffer> buf;
      assert(SUCCEEDED(unk.As(&buf)));

      D3D11_BUFFER_DESC desc;
      buf->GetDesc(&desc);

      (*stream) << " Buffer:";
      if (desc.BindFlags & D3D11_BIND_VERTEX_BUFFER) {
        (*stream) << " VB";
      }
      if (desc.BindFlags & D3D11_BIND_INDEX_BUFFER) {
        (*stream) << " IB";
      }
      if (desc.BindFlags & D3D11_BIND_CONSTANT_BUFFER) {
        (*stream) << " CB";
      }

      (*stream) << ";";

      std::string name;
      if (GetDebugName(buf.Get(), &name))
        (*stream) << " " << name << ";";

      break;
    }
    case EGL_ANGLE_TRACK_TYPE_SWAPCHAIN: {
      ComPtr<IDXGISwapChain> swap_chain;
      assert(SUCCEEDED(unk.As(&swap_chain)));

      DXGI_SWAP_CHAIN_DESC desc;
      swap_chain->GetDesc(&desc);

      (*stream) << " SwapChain: "
                << desc.BufferDesc.Width << "x" << desc.BufferDesc.Height
                << ";";
      break;
    }
    default:
      (*stream) << " Unknown resource;";
  }
}

void ResourceTracker(const EGLResourceTrackerDataAngle *data,
                     void *user_data) {
  static int mem_count;
  static int hit_countes;

  if (data->iOpType == EGL_ANGLE_TRACK_OPTYPE_CREATE) {
    mem_count += data->iSize;
  }
  else {
    mem_count -= data->iSize;
  }

  std::stringstream info;
  GetResourceData(data, &info);
  std::cout << "VMem: " << (mem_count >> 20) << "MB" << info.str();
}

void InitializeMemoryTracking() {
  PFNEGLSETRESOURCETRACKERANGLEPROC eglSetResourceTrackerANGLE =
      reinterpret_cast<PFNEGLSETRESOURCETRACKERANGLEPROC>(eglGetProcAddress(
          "eglSetResourceTrackerANGLE"));

  assert(eglSetResourceTrackerANGLE);

  eglSetResourceTrackerANGLE(g_display, &ResourceTracker, this);
}
