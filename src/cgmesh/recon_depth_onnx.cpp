#include "recon_depth_onnx.h"

#ifdef CG_HAS_ONNX

#include "onnx_session.h"

#include "../cgimg/cgimg.h"

#include <memory>
#include <vector>

namespace recon {

// Normalisation ImageNet (RGB) attendue par Depth Anything V2.
static const float kMean[3] = {0.485f, 0.456f, 0.406f};
static const float kStd[3]  = {0.229f, 0.224f, 0.225f};

// Le runtime ORT vit désormais dans cgml::OnnxSession (onnx_session.{h,cpp}) :
// ce TU ne contient plus que l'adaptateur métier (Img -> NCHW/ImageNet, sortie
// -> DepthMap) et n'inclut plus aucun en-tête ONNX Runtime.
struct OnnxDepthSource::Impl
{
    std::unique_ptr<cgml::OnnxSession> session;
};

OnnxDepthSource::OnnxDepthSource(const std::string& modelPath, int inputSize)
{
    m_inputSize = (inputSize / 14) * 14;
    if (m_inputSize < 14) m_inputSize = 518;

    m_impl = new Impl();
    // Les options par défaut reproduisent le comportement actuel :
    // intraOpThreads = hardware_concurrency(), GraphOptimizationLevel ORT_ENABLE_ALL.
    m_impl->session = cgml::OnnxSession::load(modelPath, {});
}

OnnxDepthSource::~OnnxDepthSource()
{
    delete m_impl;
}

bool OnnxDepthSource::ok() const
{
    return m_impl && m_impl->session && m_impl->session->ok();
}

DepthMap OnnxDepthSource::estimate(const Img& image)
{
    DepthMap dm;
    if (!ok())
        return dm;

    const int S = m_inputSize;

    // Rééchantillonnage S x S (bilinéaire, via Img::resize désormais correct) +
    // normalisation ImageNet, agencement NCHW.
    Img tmp(image);
    tmp.resize(static_cast<unsigned>(S), static_cast<unsigned>(S), 1);

    std::vector<float> in(static_cast<size_t>(3) * S * S);
    for (int y = 0; y < S; ++y)
        for (int x = 0; x < S; ++x)
        {
            unsigned char r, g, b, a;
            tmp.get_pixel(static_cast<unsigned>(x), static_cast<unsigned>(y),
                          &r, &g, &b, &a);
            const float rgb[3] = {r / 255.f, g / 255.f, b / 255.f};
            for (int c = 0; c < 3; ++c)
                in[static_cast<size_t>(c) * S * S + static_cast<size_t>(y) * S + x] =
                    (rgb[c] - kMean[c]) / kStd[c];
        }

    std::vector<int64_t> outShape;
    std::vector<float> out = m_impl->session->run(
        in.data(), {1, 3, S, S}, &outShape);

    dm.w = S;
    dm.h = S;
    dm.isDisparity = true;
    dm.z = std::move(out);   // sortie [1,S,S] -> S*S valeurs
    return dm;
}

} // namespace recon

#endif // CG_HAS_ONNX
