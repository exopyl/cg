#pragma once
#ifdef CG_HAS_ONNX

#include <string>
#include <vector>
#include <cstdint>
#include <memory>

namespace cgml {

struct SessionOptions {
    int  intraOpThreads = 0;    // 0 -> std::thread::hardware_concurrency()
    bool optimizeAll    = true; // GraphOptimizationLevel::ORT_ENABLE_ALL
};

// Wrapper ONNX Runtime pur : ne connaît AUCUN type cgimg/cgmesh.
// Header sans en-tête ORT (pimpl) -> includable même hors build ONNX consommateur.
// Surface volontairement minimale (float32, mono-entrée/mono-sortie, CPU) : calquée
// sur l'usage actuel. Extensible (DType, multi-I/O, zéro-copie) sans casse.
class OnnxSession {
public:
    // Fabrique. Renvoie nullptr si le modèle ne charge pas (parité avec ok()=false).
    static std::unique_ptr<OnnxSession> load(const std::string& modelPath,
                                             const SessionOptions& opts = {});
    ~OnnxSession();

    OnnxSession(const OnnxSession&)            = delete; // possède la session ORT
    OnnxSession& operator=(const OnnxSession&) = delete;

    bool               ok()         const;
    const std::string& inputName()  const;
    const std::string& outputName() const;

    // Exécute une inférence float32 dense (row-major).
    // `shape` = dims de l'entrée (ex. {1,3,518,518}). `outShape` (optionnel) reçoit
    // la shape de sortie. Retourne le buffer de sortie (copie, parité avec l'existant).
    std::vector<float> run(const float* data, const std::vector<int64_t>& shape,
                           std::vector<int64_t>* outShape = nullptr);

private:
    OnnxSession();                 // via load()
    struct Impl;                   // Ort::Env / Ort::Session : défini dans le .cpp
    Impl* m_impl = nullptr;
};

} // namespace cgml

#endif // CG_HAS_ONNX
