#include "recon_depth_onnx.h"

#ifdef CG_HAS_ONNX

#include <onnxruntime_cxx_api.h>

#include "../cgimg/cgimg.h"

#include <algorithm>
#include <thread>
#include <vector>

namespace recon {

// Normalisation ImageNet (RGB) attendue par Depth Anything V2.
static const float kMean[3] = {0.485f, 0.456f, 0.406f};
static const float kStd[3]  = {0.229f, 0.224f, 0.225f};

struct OnnxDepthSource::Impl
{
    Ort::Env            env{ORT_LOGGING_LEVEL_WARNING, "cgmesh_onnx"};
    Ort::SessionOptions opts;
    Ort::Session*       session = nullptr;
    std::string         inName, outName;
    bool                ok = false;
};

OnnxDepthSource::OnnxDepthSource(const std::string& modelPath, int inputSize)
{
    m_inputSize = (inputSize / 14) * 14;
    if (m_inputSize < 14) m_inputSize = 518;

    m_impl = new Impl();
    unsigned hw = std::thread::hardware_concurrency();
    m_impl->opts.SetIntraOpNumThreads(hw == 0 ? 1 : static_cast<int>(hw));
    m_impl->opts.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);

    try
    {
#ifdef _WIN32
        std::wstring wpath(modelPath.begin(), modelPath.end());
        m_impl->session = new Ort::Session(m_impl->env, wpath.c_str(), m_impl->opts);
#else
        m_impl->session = new Ort::Session(m_impl->env, modelPath.c_str(), m_impl->opts);
#endif
        Ort::AllocatorWithDefaultOptions alloc;
        m_impl->inName  = m_impl->session->GetInputNameAllocated(0, alloc).get();
        m_impl->outName = m_impl->session->GetOutputNameAllocated(0, alloc).get();
        m_impl->ok = true;
    }
    catch (const Ort::Exception&)
    {
        m_impl->ok = false;
    }
}

OnnxDepthSource::~OnnxDepthSource()
{
    if (m_impl)
    {
        delete m_impl->session;
        delete m_impl;
    }
}

bool OnnxDepthSource::ok() const { return m_impl && m_impl->ok; }

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

    std::vector<int64_t> shape = {1, 3, S, S};
    auto mem = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
    Ort::Value t = Ort::Value::CreateTensor<float>(
        mem, in.data(), in.size(), shape.data(), shape.size());

    const char* inN[]  = {m_impl->inName.c_str()};
    const char* outN[] = {m_impl->outName.c_str()};
    auto outs = m_impl->session->Run(Ort::RunOptions{nullptr}, inN, &t, 1, outN, 1);

    const size_t cnt = outs[0].GetTensorTypeAndShapeInfo().GetElementCount();
    const float* od  = outs[0].GetTensorData<float>();

    dm.w = S;
    dm.h = S;
    dm.isDisparity = true;
    dm.z.assign(od, od + cnt);   // sortie [1,S,S] -> S*S valeurs
    return dm;
}

} // namespace recon

#endif // CG_HAS_ONNX
