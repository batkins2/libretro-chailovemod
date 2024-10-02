#include "chai_gfx.h"

namespace love
{

namespace gfx
{

Shader *wrap_newShader(const char *FileName) {
    if (instance()->isCreated()) {
        File *file = filesystemmod::Filesystem::openFile(FileName, filesystemmod::File::Mode::MODE_READ);
        Shader::CompileOptions *options;
        return instance()->newShader(file->read(), options);
    }
}

}
}