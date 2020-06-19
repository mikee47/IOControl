#include <IO/Strings.h>

namespace IO
{
#define XX(tag) DEFINE_FSTR(FS_##tag, #tag)
IO_FLASHSTRING_MAP(XX)
#undef XX

} // namespace IO
