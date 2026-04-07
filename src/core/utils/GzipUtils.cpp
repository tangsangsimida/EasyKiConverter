#include "core/utils/GzipUtils.h"

#include <QByteArray>
#include <QDebug>
#include <zlib.h>

// Gzip magic number: 0x1f 0x8b
static constexpr int GZIP_MAGIC_1 = 0x1f;
static constexpr int GZIP_MAGIC_2 = 0x8b;

bool GzipUtils::isGzipped(const QByteArray& data) {
    if (data.size() < 2) {
        return false;
    }
    const unsigned char* bytes = reinterpret_cast<const unsigned char*>(data.constData());
    return bytes[0] == GZIP_MAGIC_1 && bytes[1] == GZIP_MAGIC_2;
}

GzipUtils::DecompressResult GzipUtils::decompress(const QByteArray& compressedData) {
    if (compressedData.size() < 18) {
        // Too small to be valid gzip, return failure
        return DecompressResult();
    }

    // If not gzip, return as-is with success=true (caller should handle non-gzip data)
    if (!isGzipped(compressedData)) {
        return DecompressResult(compressedData, true);
    }

    z_stream stream;
    memset(&stream, 0, sizeof(stream));

    if (inflateInit2(&stream, 15 + 16) != Z_OK) {
        qWarning() << "Failed to initialize zlib";
        return DecompressResult();
    }

    stream.next_in = (Bytef*)compressedData.data();
    stream.avail_in = compressedData.size();
    stream.next_out = nullptr;
    stream.avail_out = 0;

    QByteArray decompressed;
    const int bufferSize = 8192;
    char buffer[bufferSize];

    int ret;
    do {
        stream.avail_out = bufferSize;
        stream.next_out = (Bytef*)buffer;

        ret = inflate(&stream, Z_NO_FLUSH);

        if (ret == Z_OK || ret == Z_STREAM_END) {
            decompressed.append(buffer, bufferSize - stream.avail_out);
        }
    } while (ret == Z_OK);

    inflateEnd(&stream);

    if (ret != Z_STREAM_END) {
        qWarning() << "Failed to decompress gzip data, error:" << ret;
        return DecompressResult();
    }

    return DecompressResult(decompressed, true);
}
