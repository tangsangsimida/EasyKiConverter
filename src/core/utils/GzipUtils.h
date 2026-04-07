#ifndef GZIPUTILS_H
#define GZIPUTILS_H

#include <QByteArray>

namespace GzipUtils {

/**
 * @brief Gzip decompression utilities
 *
 * Provides unified gzip decompression for all network components.
 * Replaces duplicate implementations in NetworkUtils, NetworkWorker, and FetchWorker.
 */

/**
 * @brief Result of gzip decompression
 */
struct DecompressResult {
    QByteArray data;  ///< Decompressed data
    bool success;  ///< true if decompression succeeded (even if data is empty)

    DecompressResult() : success(false) {}

    DecompressResult(const QByteArray& d, bool s) : data(d), success(s) {}
};

/**
 * @brief Decompress gzip data
 * @param compressedData The gzip compressed data
 * @return DecompressResult with success=false on failure, success=true otherwise (including empty content)
 */
DecompressResult decompress(const QByteArray& compressedData);

/**
 * @brief Check if data is gzip compressed
 * @param data Data to check
 * @return true if data appears to be gzip compressed
 */
bool isGzipped(const QByteArray& data);

}  // namespace GzipUtils

#endif  // GZIPUTILS_H
