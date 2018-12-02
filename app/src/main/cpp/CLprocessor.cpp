#define __CL_ENABLE_EXCEPTIONS
#define CL_USE_DEPRECATED_OPENCL_1_1_APIS /*let's give a chance for OpenCL 1.1 devices*/

#include "CL/cl.hpp"

#include <GLES2/gl2.h>
#include <EGL/egl.h>

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/core/ocl.hpp>

#include "common.hpp"

#include <stdexcept>

using namespace std;
using namespace cv;
using namespace cv::ocl;

const char oclProgB2B[] = "// clBuffer to clBuffer";
const char oclProgI2B[] = "// clImage to clBuffer";
const char oclProgI2I[] = \
  "__constant sampler_t sampler = CLK_NORMALIZED_COORDS_FALSE | CLK_ADDRESS_CLAMP_TO_EDGE | CLK_FILTER_NEAREST; \n" \
    "\n" \
    "__kernel void Laplacian( \n" \
    "        __read_only image2d_t imgIn, \n" \
    "        __write_only image2d_t imgOut \n" \
    "    ) { \n" \
    "  \n" \
    "    const int2 pos = {get_global_id(0), get_global_id(1)}; \n" \
    "  \n" \
    "    float4 sum = (float4) 0.0f; \n" \
    "    sum += read_imagef(imgIn, sampler, pos + (int2)(-1,0)); \n" \
    "    sum += read_imagef(imgIn, sampler, pos + (int2)(+1,0)); \n" \
    "    sum += read_imagef(imgIn, sampler, pos + (int2)(0,-1)); \n" \
    "    sum += read_imagef(imgIn, sampler, pos + (int2)(0,+1)); \n" \
    "    sum -= read_imagef(imgIn, sampler, pos) * 4; \n" \
    "  \n" \
    "    write_imagef(imgOut, pos, sum*10); \n" \
    "} \n";

void dumpCLinfo()
{
    LOGD("*** OpenCL info ***");
    try
    {
        std::vector<cl::Platform> platforms;
        cl::Platform::get(&platforms);
        LOGD("OpenCL info: Found %d OpenCL platforms", platforms.size());
        for (int i = 0; i < platforms.size(); ++i)
        {
            std::string name = platforms[i].getInfo<CL_PLATFORM_NAME>();
            std::string version = platforms[i].getInfo<CL_PLATFORM_VERSION>();
            std::string profile = platforms[i].getInfo<CL_PLATFORM_PROFILE>();
            std::string extensions = platforms[i].getInfo<CL_PLATFORM_EXTENSIONS>();
            LOGD( "OpenCL info: Platform[%d] = %s, ver = %s, prof = %s, ext = %s",
                  i, name.c_str(), version.c_str(), profile.c_str(), extensions.c_str() );
        }

        std::vector<cl::Device> devices;
        platforms[0].getDevices(CL_DEVICE_TYPE_ALL, &devices);

        for (int i = 0; i < devices.size(); ++i)
        {
            std::string name = devices[i].getInfo<CL_DEVICE_NAME>();
            std::string extensions = devices[i].getInfo<CL_DEVICE_EXTENSIONS>();
            cl_ulong type = devices[i].getInfo<CL_DEVICE_TYPE>();
            LOGD( "OpenCL info: Device[%d] = %s (%s), ext = %s",
                  i, name.c_str(), (type==CL_DEVICE_TYPE_GPU ? "GPU" : "CPU"), extensions.c_str() );
        }
    }
    catch(const cl::Error& e)
    {
        LOGE( "OpenCL info: error while gathering OpenCL info: %s (%d)", e.what(), e.err() );
    }
    catch(const std::exception& e)
    {
        LOGE( "OpenCL info: error while gathering OpenCL info: %s", e.what() );
    }
    catch(...)
    {
        LOGE( "OpenCL info: unknown error while gathering OpenCL info" );
    }
    LOGD("*******************");
}

namespace clp {
    cl::Context theContext;
    cl::CommandQueue theQueue;
    cl::Program theProgB2B, theProgI2B, theProgI2I;
    bool haveOpenCL = false;
}

namespace opencl {

class PlatformInfo
{
public:
    PlatformInfo()
    {}

    ~PlatformInfo()
    {}

    cl_int QueryInfo(cl_platform_id id)
    {
        query_param(id, CL_PLATFORM_PROFILE, m_profile);
        query_param(id, CL_PLATFORM_VERSION, m_version);
        query_param(id, CL_PLATFORM_NAME, m_name);
        query_param(id, CL_PLATFORM_VENDOR, m_vendor);
        query_param(id, CL_PLATFORM_EXTENSIONS, m_extensions);
        return CL_SUCCESS;
    }

    std::string Profile()    { return m_profile; }
    std::string Version()    { return m_version; }
    std::string Name()       { return m_name; }
    std::string Vendor()     { return m_vendor; }
    std::string Extensions() { return m_extensions; }

private:
    cl_int query_param(cl_platform_id id, cl_platform_info param, std::string& paramStr)
    {
        cl_int res;

        size_t psize;
        cv::AutoBuffer<char> buf;

        res = clGetPlatformInfo(id, param, 0, 0, &psize);
        if (CL_SUCCESS != res)
            //throw std::runtime_error(std::string("clGetPlatformInfo failed"));
            LOGD("clGetPlatformInfo failed");

        buf.resize(psize);
        res = clGetPlatformInfo(id, param, psize, buf, 0);
        if (CL_SUCCESS != res)
            //throw std::runtime_error(std::string("clGetPlatformInfo failed"));
            LOGD("clGetPlatformInfo failed");

        // just in case, ensure trailing zero for ASCIIZ string
        buf[psize] = 0;

        paramStr = buf;

        return CL_SUCCESS;
    }

private:
    std::string m_profile;
    std::string m_version;
    std::string m_name;
    std::string m_vendor;
    std::string m_extensions;
};


class DeviceInfo
{
public:
    DeviceInfo()
    {}

    ~DeviceInfo()
    {}

    cl_int QueryInfo(cl_device_id id)
    {
        query_param(id, CL_DEVICE_TYPE, m_type);
        query_param(id, CL_DEVICE_VENDOR_ID, m_vendor_id);
        query_param(id, CL_DEVICE_MAX_COMPUTE_UNITS, m_max_compute_units);
        query_param(id, CL_DEVICE_MAX_WORK_ITEM_DIMENSIONS, m_max_work_item_dimensions);
        query_param(id, CL_DEVICE_MAX_WORK_ITEM_SIZES, m_max_work_item_sizes);
        query_param(id, CL_DEVICE_MAX_WORK_GROUP_SIZE, m_max_work_group_size);
        query_param(id, CL_DEVICE_PREFERRED_VECTOR_WIDTH_CHAR, m_preferred_vector_width_char);
        query_param(id, CL_DEVICE_PREFERRED_VECTOR_WIDTH_SHORT, m_preferred_vector_width_short);
        query_param(id, CL_DEVICE_PREFERRED_VECTOR_WIDTH_INT, m_preferred_vector_width_int);
        query_param(id, CL_DEVICE_PREFERRED_VECTOR_WIDTH_LONG, m_preferred_vector_width_long);
        query_param(id, CL_DEVICE_PREFERRED_VECTOR_WIDTH_FLOAT, m_preferred_vector_width_float);
        query_param(id, CL_DEVICE_PREFERRED_VECTOR_WIDTH_DOUBLE, m_preferred_vector_width_double);
#if defined(CL_VERSION_1_1)
        query_param(id, CL_DEVICE_PREFERRED_VECTOR_WIDTH_HALF, m_preferred_vector_width_half);
        query_param(id, CL_DEVICE_NATIVE_VECTOR_WIDTH_CHAR, m_native_vector_width_char);
        query_param(id, CL_DEVICE_NATIVE_VECTOR_WIDTH_SHORT, m_native_vector_width_short);
        query_param(id, CL_DEVICE_NATIVE_VECTOR_WIDTH_INT, m_native_vector_width_int);
        query_param(id, CL_DEVICE_NATIVE_VECTOR_WIDTH_LONG, m_native_vector_width_long);
        query_param(id, CL_DEVICE_NATIVE_VECTOR_WIDTH_FLOAT, m_native_vector_width_float);
        query_param(id, CL_DEVICE_NATIVE_VECTOR_WIDTH_DOUBLE, m_native_vector_width_double);
        query_param(id, CL_DEVICE_NATIVE_VECTOR_WIDTH_HALF, m_native_vector_width_half);
#endif
        query_param(id, CL_DEVICE_MAX_CLOCK_FREQUENCY, m_max_clock_frequency);
        query_param(id, CL_DEVICE_ADDRESS_BITS, m_address_bits);
        query_param(id, CL_DEVICE_MAX_MEM_ALLOC_SIZE, m_max_mem_alloc_size);
        query_param(id, CL_DEVICE_IMAGE_SUPPORT, m_image_support);
        query_param(id, CL_DEVICE_MAX_READ_IMAGE_ARGS, m_max_read_image_args);
        query_param(id, CL_DEVICE_MAX_WRITE_IMAGE_ARGS, m_max_write_image_args);
#if defined(CL_VERSION_2_0)
        query_param(id, CL_DEVICE_MAX_READ_WRITE_IMAGE_ARGS, m_max_read_write_image_args);
#endif
        query_param(id, CL_DEVICE_IMAGE2D_MAX_WIDTH, m_image2d_max_width);
        query_param(id, CL_DEVICE_IMAGE2D_MAX_HEIGHT, m_image2d_max_height);
        query_param(id, CL_DEVICE_IMAGE3D_MAX_WIDTH, m_image3d_max_width);
        query_param(id, CL_DEVICE_IMAGE3D_MAX_HEIGHT, m_image3d_max_height);
        query_param(id, CL_DEVICE_IMAGE3D_MAX_DEPTH, m_image3d_max_depth);
#if defined(CL_VERSION_1_2)
        query_param(id, CL_DEVICE_IMAGE_MAX_BUFFER_SIZE, m_image_max_buffer_size);
        query_param(id, CL_DEVICE_IMAGE_MAX_ARRAY_SIZE, m_image_max_array_size);
#endif
        query_param(id, CL_DEVICE_MAX_SAMPLERS, m_max_samplers);
#if defined(CL_VERSION_1_2)
        query_param(id, CL_DEVICE_IMAGE_PITCH_ALIGNMENT, m_image_pitch_alignment);
        query_param(id, CL_DEVICE_IMAGE_BASE_ADDRESS_ALIGNMENT, m_image_base_address_alignment);
#endif
#if defined(CL_VERSION_2_0)
        query_param(id, CL_DEVICE_MAX_PIPE_ARGS, m_max_pipe_args);
        query_param(id, CL_DEVICE_PIPE_MAX_ACTIVE_RESERVATIONS, m_pipe_max_active_reservations);
        query_param(id, CL_DEVICE_PIPE_MAX_PACKET_SIZE, m_pipe_max_packet_size);
#endif
        query_param(id, CL_DEVICE_MAX_PARAMETER_SIZE, m_max_parameter_size);
        query_param(id, CL_DEVICE_MEM_BASE_ADDR_ALIGN, m_mem_base_addr_align);
        query_param(id, CL_DEVICE_SINGLE_FP_CONFIG, m_single_fp_config);
#if defined(CL_VERSION_1_2)
        query_param(id, CL_DEVICE_DOUBLE_FP_CONFIG, m_double_fp_config);
#endif
        query_param(id, CL_DEVICE_GLOBAL_MEM_CACHE_TYPE, m_global_mem_cache_type);
        query_param(id, CL_DEVICE_GLOBAL_MEM_CACHELINE_SIZE, m_global_mem_cacheline_size);
        query_param(id, CL_DEVICE_GLOBAL_MEM_CACHE_SIZE, m_global_mem_cache_size);
        query_param(id, CL_DEVICE_GLOBAL_MEM_SIZE, m_global_mem_size);
        query_param(id, CL_DEVICE_MAX_CONSTANT_BUFFER_SIZE, m_max_constant_buffer_size);
        query_param(id, CL_DEVICE_MAX_CONSTANT_ARGS, m_max_constant_args);
#if defined(CL_VERSION_2_0)
        query_param(id, CL_DEVICE_MAX_GLOBAL_VARIABLE_SIZE, m_max_global_variable_size);
        query_param(id, CL_DEVICE_GLOBAL_VARIABLE_PREFERRED_TOTAL_SIZE, m_global_variable_preferred_total_size);
#endif
        query_param(id, CL_DEVICE_LOCAL_MEM_TYPE, m_local_mem_type);
        query_param(id, CL_DEVICE_LOCAL_MEM_SIZE, m_local_mem_size);
        query_param(id, CL_DEVICE_ERROR_CORRECTION_SUPPORT, m_error_correction_support);
#if defined(CL_VERSION_1_1)
        query_param(id, CL_DEVICE_HOST_UNIFIED_MEMORY, m_host_unified_memory);
#endif
        query_param(id, CL_DEVICE_PROFILING_TIMER_RESOLUTION, m_profiling_timer_resolution);
        query_param(id, CL_DEVICE_ENDIAN_LITTLE, m_endian_little);
        query_param(id, CL_DEVICE_AVAILABLE, m_available);
        query_param(id, CL_DEVICE_COMPILER_AVAILABLE, m_compiler_available);
#if defined(CL_VERSION_1_2)
        query_param(id, CL_DEVICE_LINKER_AVAILABLE, m_linker_available);
#endif
        query_param(id, CL_DEVICE_EXECUTION_CAPABILITIES, m_execution_capabilities);
        query_param(id, CL_DEVICE_QUEUE_PROPERTIES, m_queue_properties);
#if defined(CL_VERSION_2_0)
        query_param(id, CL_DEVICE_QUEUE_ON_HOST_PROPERTIES, m_queue_on_host_properties);
        query_param(id, CL_DEVICE_QUEUE_ON_DEVICE_PROPERTIES, m_queue_on_device_properties);
        query_param(id, CL_DEVICE_QUEUE_ON_DEVICE_PREFERRED_SIZE, m_queue_on_device_preferred_size);
        query_param(id, CL_DEVICE_QUEUE_ON_DEVICE_MAX_SIZE, m_queue_on_device_max_size);
        query_param(id, CL_DEVICE_MAX_ON_DEVICE_QUEUES, m_max_on_device_queues);
        query_param(id, CL_DEVICE_MAX_ON_DEVICE_EVENTS, m_max_on_device_events);
#endif
#if defined(CL_VERSION_1_2)
        query_param(id, CL_DEVICE_BUILT_IN_KERNELS, m_built_in_kernels);
#endif
        query_param(id, CL_DEVICE_PLATFORM, m_platform);
        query_param(id, CL_DEVICE_NAME, m_name);
        query_param(id, CL_DEVICE_VENDOR, m_vendor);
        query_param(id, CL_DRIVER_VERSION, m_driver_version);
        query_param(id, CL_DEVICE_PROFILE, m_profile);
        query_param(id, CL_DEVICE_VERSION, m_version);
#if defined(CL_VERSION_1_1)
        query_param(id, CL_DEVICE_OPENCL_C_VERSION, m_opencl_c_version);
#endif
        query_param(id, CL_DEVICE_EXTENSIONS, m_extensions);
#if defined(CL_VERSION_1_2)
        query_param(id, CL_DEVICE_PRINTF_BUFFER_SIZE, m_printf_buffer_size);
        query_param(id, CL_DEVICE_PREFERRED_INTEROP_USER_SYNC, m_preferred_interop_user_sync);
        query_param(id, CL_DEVICE_PARENT_DEVICE, m_parent_device);
        query_param(id, CL_DEVICE_PARTITION_MAX_SUB_DEVICES, m_partition_max_sub_devices);
        query_param(id, CL_DEVICE_PARTITION_PROPERTIES, m_partition_properties);
        query_param(id, CL_DEVICE_PARTITION_AFFINITY_DOMAIN, m_partition_affinity_domain);
        query_param(id, CL_DEVICE_PARTITION_TYPE, m_partition_type);
        query_param(id, CL_DEVICE_REFERENCE_COUNT, m_reference_count);
#endif
        return CL_SUCCESS;
    }

    std::string Name() { return m_name; }

private:
    template<typename T>
    cl_int query_param(cl_device_id id, cl_device_info param, T& value)
    {
        cl_int res;
        size_t size = 0;

        res = clGetDeviceInfo(id, param, 0, 0, &size);
        if (CL_SUCCESS != res && size != 0)
            //throw std::runtime_error(std::string("clGetDeviceInfo failed"));
            LOGE("clGetDeviceInfo failed");

        if (0 == size)
            return CL_SUCCESS;

        if (sizeof(T) != size)
            //throw std::runtime_error(std::string("clGetDeviceInfo: param size mismatch"));
            LOGE("clGetDeviceInfo: param size mismatch");

        res = clGetDeviceInfo(id, param, size, &value, 0);
        if (CL_SUCCESS != res)
            //throw std::runtime_error(std::string("clGetDeviceInfo failed"));
            LOGE("clGetDeviceInfo failed");

        return CL_SUCCESS;
    }

    template<typename T>
    cl_int query_param(cl_device_id id, cl_device_info param, std::vector<T>& value)
    {
        cl_int res;
        size_t size;

        res = clGetDeviceInfo(id, param, 0, 0, &size);
        if (CL_SUCCESS != res)
            //throw std::runtime_error(std::string("clGetDeviceInfo failed"));
            LOGE("clGetDeviceInfo failed");


        if (0 == size)
            return CL_SUCCESS;

        value.resize(size / sizeof(T));

        res = clGetDeviceInfo(id, param, size, &value[0], 0);
        if (CL_SUCCESS != res)
            //throw std::runtime_error(std::string("clGetDeviceInfo failed"));
            LOGE("clGetDeviceInfo failed");

        return CL_SUCCESS;
    }

    cl_int query_param(cl_device_id id, cl_device_info param, std::string& value)
    {
        cl_int res;
        size_t size;

        res = clGetDeviceInfo(id, param, 0, 0, &size);
        if (CL_SUCCESS != res)
            //throw std::runtime_error(std::string("clGetDeviceInfo failed"));
            LOGE("clGetDeviceInfo failed");

        value.resize(size + 1);

        res = clGetDeviceInfo(id, param, size, &value[0], 0);
        if (CL_SUCCESS != res)
            //throw std::runtime_error(std::string("clGetDeviceInfo failed"));
            LOGE("clGetDeviceInfo failed");

        // just in case, ensure trailing zero for ASCIIZ string
        value[size] = 0;

        return CL_SUCCESS;
    }

private:
    cl_device_type                            m_type;
    cl_uint                                   m_vendor_id;
    cl_uint                                   m_max_compute_units;
    cl_uint                                   m_max_work_item_dimensions;
    std::vector<size_t>                       m_max_work_item_sizes;
    size_t                                    m_max_work_group_size;
    cl_uint                                   m_preferred_vector_width_char;
    cl_uint                                   m_preferred_vector_width_short;
    cl_uint                                   m_preferred_vector_width_int;
    cl_uint                                   m_preferred_vector_width_long;
    cl_uint                                   m_preferred_vector_width_float;
    cl_uint                                   m_preferred_vector_width_double;
#if defined(CL_VERSION_1_1)
    cl_uint                                   m_preferred_vector_width_half;
    cl_uint                                   m_native_vector_width_char;
    cl_uint                                   m_native_vector_width_short;
    cl_uint                                   m_native_vector_width_int;
    cl_uint                                   m_native_vector_width_long;
    cl_uint                                   m_native_vector_width_float;
    cl_uint                                   m_native_vector_width_double;
    cl_uint                                   m_native_vector_width_half;
#endif
    cl_uint                                   m_max_clock_frequency;
    cl_uint                                   m_address_bits;
    cl_ulong                                  m_max_mem_alloc_size;
    cl_bool                                   m_image_support;
    cl_uint                                   m_max_read_image_args;
    cl_uint                                   m_max_write_image_args;
#if defined(CL_VERSION_2_0)
    cl_uint                                   m_max_read_write_image_args;
#endif
    size_t                                    m_image2d_max_width;
    size_t                                    m_image2d_max_height;
    size_t                                    m_image3d_max_width;
    size_t                                    m_image3d_max_height;
    size_t                                    m_image3d_max_depth;
#if defined(CL_VERSION_1_2)
    size_t                                    m_image_max_buffer_size;
    size_t                                    m_image_max_array_size;
#endif
    cl_uint                                   m_max_samplers;
#if defined(CL_VERSION_1_2)
    cl_uint                                   m_image_pitch_alignment;
    cl_uint                                   m_image_base_address_alignment;
#endif
#if defined(CL_VERSION_2_0)
    cl_uint                                   m_max_pipe_args;
    cl_uint                                   m_pipe_max_active_reservations;
    cl_uint                                   m_pipe_max_packet_size;
#endif
    size_t                                    m_max_parameter_size;
    cl_uint                                   m_mem_base_addr_align;
    cl_device_fp_config                       m_single_fp_config;
#if defined(CL_VERSION_1_2)
    cl_device_fp_config                       m_double_fp_config;
#endif
    cl_device_mem_cache_type                  m_global_mem_cache_type;
    cl_uint                                   m_global_mem_cacheline_size;
    cl_ulong                                  m_global_mem_cache_size;
    cl_ulong                                  m_global_mem_size;
    cl_ulong                                  m_max_constant_buffer_size;
    cl_uint                                   m_max_constant_args;
#if defined(CL_VERSION_2_0)
    size_t                                    m_max_global_variable_size;
    size_t                                    m_global_variable_preferred_total_size;
#endif
    cl_device_local_mem_type                  m_local_mem_type;
    cl_ulong                                  m_local_mem_size;
    cl_bool                                   m_error_correction_support;
#if defined(CL_VERSION_1_1)
    cl_bool                                   m_host_unified_memory;
#endif
    size_t                                    m_profiling_timer_resolution;
    cl_bool                                   m_endian_little;
    cl_bool                                   m_available;
    cl_bool                                   m_compiler_available;
#if defined(CL_VERSION_1_2)
    cl_bool                                   m_linker_available;
#endif
    cl_device_exec_capabilities               m_execution_capabilities;
    cl_command_queue_properties               m_queue_properties;
#if defined(CL_VERSION_2_0)
    cl_command_queue_properties               m_queue_on_host_properties;
    cl_command_queue_properties               m_queue_on_device_properties;
    cl_uint                                   m_queue_on_device_preferred_size;
    cl_uint                                   m_queue_on_device_max_size;
    cl_uint                                   m_max_on_device_queues;
    cl_uint                                   m_max_on_device_events;
#endif
#if defined(CL_VERSION_1_2)
    std::string                               m_built_in_kernels;
#endif
    cl_platform_id                            m_platform;
    std::string                               m_name;
    std::string                               m_vendor;
    std::string                               m_driver_version;
    std::string                               m_profile;
    std::string                               m_version;
#if defined(CL_VERSION_1_1)
    std::string                               m_opencl_c_version;
#endif
    std::string                               m_extensions;
#if defined(CL_VERSION_1_2)
    size_t                                    m_printf_buffer_size;
    cl_bool                                   m_preferred_interop_user_sync;
    cl_device_id                              m_parent_device;
    cl_uint                                   m_partition_max_sub_devices;
    std::vector<cl_device_partition_property> m_partition_properties;
    cl_device_affinity_domain                 m_partition_affinity_domain;
    std::vector<cl_device_partition_property> m_partition_type;
    cl_uint                                   m_reference_count;
#endif
};

} // namespace opencl


opencl::PlatformInfo        m_platformInfo;
opencl::DeviceInfo          m_deviceInfo;
std::vector<cl_platform_id> m_platform_ids;
cl_context m_context;
cl_device_id                m_device_id;
cl_command_queue            m_queue;
cl_program                  m_program;
cl_kernel                   m_kernelBuf;
cl_kernel                   m_kernelImg;
extern "C" void initCL()
{
    dumpCLinfo();

    cl_int res = CL_SUCCESS;
    cl_uint num_entries = 0;

    res = clGetPlatformIDs(0, 0, &num_entries);
    if (CL_SUCCESS != res)
        return;

    m_platform_ids.resize(num_entries);

    res = clGetPlatformIDs(num_entries, &m_platform_ids[0], 0);
    if (CL_SUCCESS != res)
        return;

    unsigned int i;

    // create context from first platform with GPU device
    for (i = 0; i < m_platform_ids.size(); i++)
    {
        cl_context_properties props[] =
        {
            CL_CONTEXT_PLATFORM,
            (cl_context_properties)(m_platform_ids[i]),
            0
        };

        m_context = clCreateContextFromType(props, CL_DEVICE_TYPE_GPU, 0, 0, &res);
        if (0 == m_context || CL_SUCCESS != res)
            continue;

        res = clGetContextInfo(m_context, CL_CONTEXT_DEVICES, sizeof(cl_device_id), &m_device_id, 0);
        if (CL_SUCCESS != res)
            return;

        m_queue = clCreateCommandQueue(m_context, m_device_id, 0, &res);
        if (0 == m_queue || CL_SUCCESS != res)
            return;

        const char* kernelSrc =
            "__kernel "
            "void bitwise_inv_buf_8uC1("
            "    __global unsigned char* pSrcDst,"
            "             int            srcDstStep,"
            "             int            rows,"
            "             int            cols)"
            "{"
            "    int x = get_global_id(0);"
            "    int y = get_global_id(1);"
            "    int idx = mad24(y, srcDstStep, x);"
            "    pSrcDst[idx] = ~pSrcDst[idx];"
            "}"
            "__kernel "
            "void bitwise_inv_img_8uC1("
            "    read_only  image2d_t srcImg,"
            "    write_only image2d_t dstImg)"
            "{"
            "    int x = get_global_id(0);"
            "    int y = get_global_id(1);"
            "    int2 coord = (int2)(x, y);"
            "    uint4 val = read_imageui(srcImg, coord);"
            "    val.x = (~val.x) & 0x000000FF;"
            "    write_imageui(dstImg, coord, val);"
            "}";
        size_t len = strlen(kernelSrc);
        m_program = clCreateProgramWithSource(m_context, 1, &kernelSrc, &len, &res);
        if (0 == m_program || CL_SUCCESS != res)
            return;

        res = clBuildProgram(m_program, 1, &m_device_id, 0, 0, 0);
        if (CL_SUCCESS != res)
            return;

        m_kernelBuf = clCreateKernel(m_program, "bitwise_inv_buf_8uC1", &res);
        if (0 == m_kernelBuf || CL_SUCCESS != res)
            return;

        m_kernelImg = clCreateKernel(m_program, "bitwise_inv_img_8uC1", &res);
        if (0 == m_kernelImg || CL_SUCCESS != res)
            return;

        m_platformInfo.QueryInfo(m_platform_ids[i]);
        m_deviceInfo.QueryInfo(m_device_id);

        // attach OpenCL context to OpenCV
        cv::ocl::attachContext(m_platformInfo.Name(), m_platform_ids[i], m_context, m_device_id);

        break;
    }

    LOGD("%d", m_context != 0 ? CL_SUCCESS : -1);

    /*
    EGLDisplay mEglDisplay = eglGetCurrentDisplay();
    if (mEglDisplay == EGL_NO_DISPLAY)
        LOGE("initCL: eglGetCurrentDisplay() returned 'EGL_NO_DISPLAY', error = %x", eglGetError());

    EGLContext mEglContext = eglGetCurrentContext();
    if (mEglContext == EGL_NO_CONTEXT)
        LOGE("initCL: eglGetCurrentContext() returned 'EGL_NO_CONTEXT', error = %x", eglGetError());

    cl_context_properties props[] =
    {   CL_GL_CONTEXT_KHR,   (cl_context_properties) mEglContext,
        CL_EGL_DISPLAY_KHR,  (cl_context_properties) mEglDisplay,
        CL_CONTEXT_PLATFORM, 0,
        0 };

    try
    {
        clp::haveOpenCL = false;
        cl::Platform p = cl::Platform::getDefault();
        std::string ext = p.getInfo<CL_PLATFORM_EXTENSIONS>();
        if(ext.find("cl_khr_gl_sharing") == std::string::npos)
            LOGE("Warning: CL-GL sharing isn't supported by PLATFORM");
        props[5] = (cl_context_properties) p();

        clp::theContext = cl::Context(CL_DEVICE_TYPE_GPU, props);
        std::vector<cl::Device> devs = clp::theContext.getInfo<CL_CONTEXT_DEVICES>();
        LOGD("Context returned %d devices, taking the 1st one", (int)devs.size());
        ext = devs[0].getInfo<CL_DEVICE_EXTENSIONS>();
        if(ext.find("cl_khr_gl_sharing") == std::string::npos)
            LOGE("Warning: CL-GL sharing isn't supported by DEVICE");

        clp::theQueue = cl::CommandQueue(clp::theContext, devs[0]);


        cl::Program::Sources src(1, std::make_pair(oclProgI2I, sizeof(oclProgI2I)));
        clp::theProgI2I = cl::Program(clp::theContext, src);
        clp::theProgI2I.build(devs);

        cv::ocl::attachContext(p.getInfo<CL_PLATFORM_NAME>(), p(), clp::theContext(), devs[0]());
        if( cv::ocl::useOpenCL() )
            LOGD("OpenCV+OpenCL works OK!");
        else
            LOGE("Can't init OpenCV with OpenCL TAPI");
        clp::haveOpenCL = true;
    }
    catch(const cl::Error& e)
    {
        LOGE("cv::ocl::Error: %s (%d)", e.what(), e.err());
    }
    catch(const std::exception& e)
    {
        LOGE("std::exception: %s", e.what());
    }
    catch(...)
    {
        LOGE( "OpenCL info: unknown error while initializing OpenCL stuff" );
    }
    LOGD("initCL completed");
    */
}

extern "C" void closeCL()
{
}

#define GL_TEXTURE_2D 0x0DE1
void procOCL_I2I(int texIn, int texOut, int w, int h)
{
    LOGD("Processing OpenCL Direct (image2d)");
    if(!clp::haveOpenCL)
    {
        LOGE("OpenCL isn't initialized");
        return;
    }

    LOGD("procOCL_I2I(%d, %d, %d, %d)", texIn, texOut, w, h);
    cl::ImageGL imgIn (clp::theContext, CL_MEM_READ_ONLY,  GL_TEXTURE_2D, 0, texIn);
    cl::ImageGL imgOut(clp::theContext, CL_MEM_WRITE_ONLY, GL_TEXTURE_2D, 0, texOut);
    std::vector < cl::Memory > images;
    images.push_back(imgIn);
    images.push_back(imgOut);

    int64_t t = getTimeMs();
    clp::theQueue.enqueueAcquireGLObjects(&images);
    clp::theQueue.finish();
    LOGD("enqueueAcquireGLObjects() costs %d ms", getTimeInterval(t));

    t = getTimeMs();
    cl::Kernel Laplacian(clp::theProgI2I, "Laplacian"); //TODO: may be done once
    Laplacian.setArg(0, imgIn);
    Laplacian.setArg(1, imgOut);
    clp::theQueue.finish();
    LOGD("Kernel() costs %d ms", getTimeInterval(t));

    t = getTimeMs();
    clp::theQueue.enqueueNDRangeKernel(Laplacian, cl::NullRange, cl::NDRange(w, h), cl::NullRange);
    clp::theQueue.finish();
    LOGD("enqueueNDRangeKernel() costs %d ms", getTimeInterval(t));

    t = getTimeMs();
    clp::theQueue.enqueueReleaseGLObjects(&images);
    clp::theQueue.finish();
    LOGD("enqueueReleaseGLObjects() costs %d ms", getTimeInterval(t));
}

void procOCL_OCV(int texIn, int texOut, int w, int h)
{
    LOGD("Processing OpenCL via OpenCV");
    if(!clp::haveOpenCL)
    {
        LOGE("OpenCL isn't initialized");
        return;
    }

    int64_t t = getTimeMs();
    cl::ImageGL imgIn (clp::theContext, CL_MEM_READ_ONLY,  GL_TEXTURE_2D, 0, texIn);
    std::vector < cl::Memory > images(1, imgIn);
    clp::theQueue.enqueueAcquireGLObjects(&images);
    clp::theQueue.finish();
    cv::UMat uIn, uOut, uTmp;
    cv::ocl::convertFromImage(imgIn(), uIn);
    LOGD("loading texture data to OpenCV UMat costs %d ms", getTimeInterval(t));
    clp::theQueue.enqueueReleaseGLObjects(&images);

    t = getTimeMs();
    //cv::blur(uIn, uOut, cv::Size(5, 5));
    cv::Laplacian(uIn, uTmp, CV_8U);
    cv:multiply(uTmp, 10, uOut);
    cv::ocl::finish();
    LOGD("OpenCV processing costs %d ms", getTimeInterval(t));

    t = getTimeMs();
    cl::ImageGL imgOut(clp::theContext, CL_MEM_WRITE_ONLY, GL_TEXTURE_2D, 0, texOut);
    images.clear();
    images.push_back(imgOut);
    clp::theQueue.enqueueAcquireGLObjects(&images);
    cl_mem clBuffer = (cl_mem)uOut.handle(cv::ACCESS_READ);
    cl_command_queue q = (cl_command_queue)cv::ocl::Queue::getDefault().ptr();
    size_t offset = 0;
    size_t origin[3] = { 0, 0, 0 };
    size_t region[3] = { (size_t) w, (size_t) h, 1 };
    CV_Assert(clEnqueueCopyBufferToImage (q, clBuffer, imgOut(), offset, origin, region, 0, NULL, NULL) == CL_SUCCESS);
    clp::theQueue.enqueueReleaseGLObjects(&images);
    cv::ocl::finish();
    LOGD("uploading results to texture costs %d ms", getTimeInterval(t));
}

void drawFrameProcCPU(int w, int h, int texOut)
{
    LOGD("Processing on CPU");
    int64_t t;

    // let's modify pixels in FBO texture in C++ code (on CPU)
    static cv::Mat m;
    m.create(h, w, CV_8UC4);

    // read
    t = getTimeMs();
    // expecting FBO to be bound
    glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, m.data);
    LOGD("glReadPixels() costs %d ms", getTimeInterval(t));

   // modify
    t = getTimeMs();
    cv::Laplacian(m, m, CV_8U);
    m *= 10;
    LOGD("Laplacian() costs %d ms", getTimeInterval(t));

    // write back
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texOut);
    t = getTimeMs();
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, m.data);
    LOGD("glTexSubImage2D() costs %d ms", getTimeInterval(t));
}


enum ProcMode {PROC_MODE_NO_PROC=0, PROC_MODE_CPU=1, PROC_MODE_OCL_DIRECT=2, PROC_MODE_OCL_OCV=3};

extern "C" void processFrame(int tex1, int tex2, int w, int h, int mode)
{
    switch(mode)
    {
        //case PROC_MODE_NO_PROC:
    case PROC_MODE_CPU:
        drawFrameProcCPU(w, h, tex2);
        break;
    case PROC_MODE_OCL_DIRECT:
        procOCL_I2I(tex1, tex2, w, h);
        break;
    case PROC_MODE_OCL_OCV:
        procOCL_OCV(tex1, tex2, w, h);
        break;
    default:
        LOGE("Unexpected processing mode: %d", mode);
    }
}
