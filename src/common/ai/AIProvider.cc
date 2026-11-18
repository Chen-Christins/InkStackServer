#include "common/ai/AIProvider.hpp"

#include "common/ai/ClaudeProvider.hpp"
#include "common/ai/DeepSeekProvider.hpp"
#include "common/ai/GLMProvider.hpp"
#include "common/ai/OpenAIProvider.hpp"

namespace blog {
namespace ai {

AIProvider::ptr createProvider(const std::string& type)
{
    if (type == "claude") {
        return std::make_shared<ClaudeProvider>();
    }
    if (type == "deepseek") {
        return std::make_shared<DeepSeekProvider>();
    }
    if (type == "glm") {
        return std::make_shared<GLMProvider>();
    }
    return std::make_shared<OpenAIProvider>(); // 默认 OpenAI
}

} // namespace ai
} // namespace blog
