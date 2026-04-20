#ifndef COMPLETIONGENERATOR_H
#define COMPLETIONGENERATOR_H

#include <QString>

namespace EasyKiConverter {

/**
 * @brief Shell 补全脚本生成器
 *
 * 为 bash、zsh、fish 生成自动补全脚本。
 */
class CompletionGenerator {
public:
    /**
     * @brief Shell 类型枚举
     */
    enum class Shell { Bash, Zsh, Fish };

    /**
     * @brief 生成补全脚本
     * @param shell Shell 类型
     * @return 补全脚本内容
     */
    static QString generate(Shell shell);

    /**
     * @brief 从字符串解析 Shell 类型
     * @param shellStr Shell 名称 (bash/zsh/fish)
     * @return Shell 类型
     */
    static Shell parseShell(const QString& shellStr);

    /**
     * @brief 获取安装说明
     * @param shell Shell 类型
     * @return 安装说明文本
     */
    static QString getInstallInstructions(Shell shell);

private:
    static QString generateBash();
    static QString generateZsh();
    static QString generateFish();
};

}  // namespace EasyKiConverter

#endif  // COMPLETIONGENERATOR_H
