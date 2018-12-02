cd ~/Android/Sdk/platform-tools/
./adb pull /vendor/lib/libOpenCL.so



To fix the sdk::__ndk1 issue do the following.
go into ~/Android/Sdk/ndk-bundle/sources/cxx-stl/llvm-libc++/include and edit the __config file and comment out all occurences of this code

#define _LIBCPP_BEGIN_NAMESPACE_STD namespace std {inline namespace _LIBCPP_NAMESPACE {                                                                                                                      
#define _LIBCPP_END_NAMESPACE_STD  } }                                                                                                                                                                       
#define _VSTD std::_LIBCPP_NAMESPACE                                                                                                                                                                         
namespace std {
  inline namespace LIBCPPNAMESPACE {
  }
}

#define _LIBCPP_BEGIN_NAMESPACE_STD namespace std { //inline namespace _LIBCPP_NAMESPACE {
#define _LIBCPP_END_NAMESPACE_STD  } //}
#define _VSTD std//::_LIBCPP_NAMESPACE
namespace std {
  //inline namespace _LIBCPP_NAMESPACE {
  //}
}
