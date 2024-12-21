#include "chai_shader.h"
#include "math/MathModule.h"

namespace love
{

chai_shader::chai_shader() {

}


chai_shader::~chai_shader() {
    shader = NULL;
    fragmentShader = NULL;
    instance = NULL;
}

chai_shader::chai_shader(const chai_shader &c) {
    shader = c.shader;
    fragmentShader = c.fragmentShader;
    instance = c.instance;
}

chai_shader *chai_shader::clone() const
{
	return new chai_shader(*this);
}

void chai_shader::newShader(love::gfx::Graphics *inst, std::vector<std::string> lines, love::gfx::Shader::CompileOptions options) {
    instance = inst;
    if (instance->isCreated()) {
        shader = instance->newShader(lines, options);
    }
}

void chai_shader::newFragmentShader(love::gfx::Graphics *inst, std::vector<std::string> lines, love::gfx::Shader::CompileOptions options) {
    instance = inst;
    if (instance->isCreated()) {
        fragmentShader = instance->newShader(lines, options);
    }
}

void chai_shader::send(const std::string &uniform, const std::vector<chaiscript::Boxed_Value> &data) {
    if (instance->isCreated()) {
        // int startidx = 0;
        auto info = shader->getUniformInfo(uniform);
        if (info->baseType == gfx::Shader::UNIFORM_SAMPLER || info->baseType == gfx::Shader::UNIFORM_STORAGETEXTURE
            || info->baseType == gfx::Shader::UNIFORM_TEXELBUFFER || info->baseType == gfx::Shader::UNIFORM_STORAGEBUFFER)
            return;

        mathmod::Transform::MatrixLayout layout = mathmod::Transform::MATRIX_ROW_MAJOR;
        // int dataidx = startidx;
        // if (info->baseType == gfx::Shader::UNIFORM_MATRIX)
        // {

        //     if (lua_type(L, startidx) == LUA_TSTRING)
        //     {
        //         // (matrixlayout, data, ...)
        //         const char *layoutstr = lua_tostring(L, startidx);
        //         if (!math::Transform::getConstant(layoutstr, layout))
        //             return luax_enumerror(L, "matrix layout", math::Transform::getConstants(layout), layoutstr);

        //         startidx++;
        //         dataidx = startidx;
        //     }
        //     else if (lua_type(L, startidx + 1) == LUA_TSTRING)
        //     {
        //         // (data, matrixlayout, ...)
        //         // Should be deprecated in the future (doesn't match the argument
        //         // order of Shader:send(name, matrixlayout, table))
        //         const char *layoutstr = lua_tostring(L, startidx + 1);
        //         if (!math::Transform::getConstant(layoutstr, layout))
        //             return luax_enumerror(L, "matrix layout", math::Transform::getConstants(layout), layoutstr);

        //         startidx++;
        //     }
        // }

        bool columnmajor = (layout == mathmod::Transform::MATRIX_COLUMN_MAJOR);
        size_t uniformstride = info->dataSize / info->count;
        int count = (int) (data.size() / uniformstride);
        const char *mem = (const char *) data.data();

        if (info->baseType != gfx::Shader::UNIFORM_MATRIX || columnmajor)
            memcpy(info->data, mem, data.size());
        else
        {
            int columns = info->matrix.columns;
            int rows = info->matrix.rows;

            const float *src = (const float *) mem;
            float *dst = info->floats;

            for (int i = 0; i < count; i++)
            {
                for (int row = 0; row < rows; row++)
                {
                    for (int column = 0; column < columns; column++)
                        dst[column * rows + row] = src[row * columns + column];
                }

                src += columns * rows;
                dst += columns * rows;
            }
        }

        if (false /*&& graphics::isGammaCorrect()*/)
        {
            // alpha is always linear (when present).
            int components = info->components;
            int gammacomponents = std::min(components, 3);
            float *values = info->floats;

            for (int i = 0; i < count; i++)
            {
                for (int j = 0; j < gammacomponents; j++)
                    values[i * components + j] = mathmod::gammaToLinear(values[i * components + j]);
            }
        }

        shader->updateUniform(info, count);
    }
}
}
