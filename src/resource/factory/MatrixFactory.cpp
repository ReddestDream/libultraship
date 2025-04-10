#include "resource/factory/MatrixFactory.h"
#include "resource/type/Matrix.h"
#include "spdlog/spdlog.h"

namespace Fast {
std::shared_ptr<Ship::IResource>
ResourceFactoryBinaryMatrixV0::ReadResource(std::shared_ptr<Ship::File> file,
                                            std::shared_ptr<Ship::ResourceInitData> initData) {
    if (!FileHasValidFormatAndReader(file, initData)) {
        return nullptr;
    }

    auto matrix = std::make_shared<Matrix>(initData);
    auto reader = std::get<std::shared_ptr<Ship::BinaryReader>>(file->Reader);

    for (size_t i = 0; i < 4; i++) {
        for (size_t j = 0; j < 4; j++) {
#ifdef GBI_FLOATS
            matrix->Matrx.mf[i][j] = reader->ReadFloat();
#else
            matrix->Matrx.m[i][j] = reader->ReadInt32();
#endif
        }
    }

    return matrix;
}
} // namespace Fast
