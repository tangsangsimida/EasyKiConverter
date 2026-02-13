#pragma once

#include <QByteArray>
#include <QString>


namespace EasyKiConverter::Test {

/**
 * @brief 基础文件访问接口，用于 Mock 文件系统
 */
class IFileProvider {
public:
    virtual ~IFileProvider() = default;
    virtual bool exists(const QString& path) const = 0;
    virtual QByteArray readAll(const QString& path) = 0;
    virtual bool writeAll(const QString& path, const QByteArray& data) = 0;
};

}  // namespace EasyKiConverter::Test
