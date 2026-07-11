#include "onnx_session.h"

#ifdef CG_HAS_ONNX

#include <onnxruntime_cxx_api.h>

#include <thread>

namespace cgml {

// Ort::Env / Ort::Session confinés à ce seul TU (pimpl).
struct OnnxSession::Impl
{
    Ort::Env            env{ORT_LOGGING_LEVEL_WARNING, "cgmesh_onnx"};
    Ort::SessionOptions opts;
    Ort::Session*       session = nullptr;
    std::string         inName, outName;
    bool                ok = false;
};

OnnxSession::OnnxSession() : m_impl(new Impl()) {}

OnnxSession::~OnnxSession()
{
    if (m_impl)
    {
        delete m_impl->session;
        delete m_impl;
    }
}

std::unique_ptr<OnnxSession> OnnxSession::load(const std::string& modelPath,
                                               const SessionOptions& sopts)
{
    std::unique_ptr<OnnxSession> s(new OnnxSession());
    Impl* impl = s->m_impl;

    int threads = sopts.intraOpThreads;
    if (threads <= 0)
    {
        unsigned hw = std::thread::hardware_concurrency();
        threads = (hw == 0) ? 1 : static_cast<int>(hw);
    }
    impl->opts.SetIntraOpNumThreads(threads);
    impl->opts.SetGraphOptimizationLevel(
        sopts.optimizeAll ? GraphOptimizationLevel::ORT_ENABLE_ALL
                          : GraphOptimizationLevel::ORT_DISABLE_ALL);

    try
    {
#ifdef _WIN32
        std::wstring wpath(modelPath.begin(), modelPath.end());
        impl->session = new Ort::Session(impl->env, wpath.c_str(), impl->opts);
#else
        impl->session = new Ort::Session(impl->env, modelPath.c_str(), impl->opts);
#endif
        Ort::AllocatorWithDefaultOptions alloc;
        impl->inName  = impl->session->GetInputNameAllocated(0, alloc).get();
        impl->outName = impl->session->GetOutputNameAllocated(0, alloc).get();
        impl->ok = true;
    }
    catch (const Ort::Exception&)
    {
        impl->ok = false;
    }

    if (!impl->ok)
        return nullptr;   // parité avec l'ancien ok()=false
    return s;
}

bool OnnxSession::ok() const { return m_impl && m_impl->ok; }
const std::string& OnnxSession::inputName()  const { return m_impl->inName; }
const std::string& OnnxSession::outputName() const { return m_impl->outName; }

std::vector<float> OnnxSession::run(const float* data, const std::vector<int64_t>& shape,
                                    std::vector<int64_t>* outShape)
{
    std::vector<float> result;
    if (!m_impl || !m_impl->ok || !m_impl->session || !data)
        return result;

    size_t count = 1;
    for (int64_t d : shape)
        count *= static_cast<size_t>(d);

    // ORT n'écrit pas dans le tenseur d'entrée : le const_cast est sûr (l'appelant
    // fournissait déjà un buffer non-const via in.data() dans l'ancien code).
    auto mem = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
    Ort::Value t = Ort::Value::CreateTensor<float>(
        mem, const_cast<float*>(data), count, shape.data(), shape.size());

    const char* inN[]  = {m_impl->inName.c_str()};
    const char* outN[] = {m_impl->outName.c_str()};
    auto outs = m_impl->session->Run(Ort::RunOptions{nullptr}, inN, &t, 1, outN, 1);

    auto info = outs[0].GetTensorTypeAndShapeInfo();
    const size_t cnt = info.GetElementCount();
    const float* od  = outs[0].GetTensorData<float>();
    result.assign(od, od + cnt);   // copie (parité avec dm.z.assign existant)

    if (outShape)
        *outShape = info.GetShape();

    return result;
}

} // namespace cgml

#endif // CG_HAS_ONNX
