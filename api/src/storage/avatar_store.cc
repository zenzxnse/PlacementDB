#include "storage/avatar_store.h"

#include "auth/secret.h"

#include <algorithm>
#include <array>
#include <fstream>
#include <string_view>
#include <system_error>
#include <utility>

namespace placedb::storage {
namespace {

constexpr std::size_t kMaximumBytes = 2U * 1024U * 1024U;

std::optional<std::pair<std::string, std::string>> ImageType(
    const std::span<const unsigned char> bytes) {
    constexpr std::array<unsigned char, 8> png{
        0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a};
    if (bytes.size() >= png.size() &&
        std::equal(png.begin(), png.end(), bytes.begin())) {
        return std::pair{"png", "image/png"};
    }
    if (bytes.size() >= 3 && bytes[0] == 0xff && bytes[1] == 0xd8 &&
        bytes[2] == 0xff) {
        return std::pair{"jpg", "image/jpeg"};
    }
    if (bytes.size() >= 12 &&
        std::string_view(reinterpret_cast<const char*>(bytes.data()), 4) == "RIFF" &&
        std::string_view(reinterpret_cast<const char*>(bytes.data() + 8), 4) == "WEBP") {
        return std::pair{"webp", "image/webp"};
    }
    return std::nullopt;
}

bool SafeKey(const std::string& key) {
    if (key.size() < 44 || key.size() > 45) return false;
    const auto dot = key.find('.');
    if (dot != 40) return false;
    for (std::size_t index = 0; index < dot; ++index) {
        const char value = key[index];
        if (!((value >= '0' && value <= '9') ||
              (value >= 'a' && value <= 'f'))) return false;
    }
    const std::string extension = key.substr(dot + 1);
    return extension == "jpg" || extension == "png" || extension == "webp";
}

}  // namespace

LocalAvatarStore::LocalAvatarStore(std::filesystem::path root)
    : root_(std::move(root)) {}

std::optional<StoredAvatar> LocalAvatarStore::Put(
    const std::span<const unsigned char> bytes) {
    if (bytes.empty() || bytes.size() > kMaximumBytes) return std::nullopt;
    const auto type = ImageType(bytes);
    if (!type.has_value()) return std::nullopt;
    std::error_code error;
    std::filesystem::create_directories(root_, error);
    if (error) return std::nullopt;
    const std::string key = auth::HashToken(auth::MintToken()).substr(0, 40) +
                            "." + type->first;
    const auto temporary = root_ / (key + ".tmp");
    const auto destination = root_ / key;
    {
        std::ofstream stream(temporary, std::ios::binary | std::ios::trunc);
        if (!stream) return std::nullopt;
        stream.write(reinterpret_cast<const char*>(bytes.data()),
                     static_cast<std::streamsize>(bytes.size()));
        if (!stream) {
            stream.close();
            std::filesystem::remove(temporary, error);
            return std::nullopt;
        }
    }
    std::filesystem::rename(temporary, destination, error);
    if (error) {
        std::filesystem::remove(temporary, error);
        return std::nullopt;
    }
    return StoredAvatar{key, type->second};
}

std::optional<std::vector<unsigned char>> LocalAvatarStore::Get(
    const std::string& key) const {
    if (!SafeKey(key)) return std::nullopt;
    std::ifstream stream(root_ / key, std::ios::binary | std::ios::ate);
    if (!stream) return std::nullopt;
    const auto size = stream.tellg();
    if (size <= 0 || size > static_cast<std::streamoff>(kMaximumBytes)) {
        return std::nullopt;
    }
    std::vector<unsigned char> bytes(static_cast<std::size_t>(size));
    stream.seekg(0);
    stream.read(reinterpret_cast<char*>(bytes.data()), size);
    if (!stream || !ImageType(bytes).has_value()) return std::nullopt;
    return bytes;
}

bool LocalAvatarStore::Remove(const std::string& key) {
    if (!SafeKey(key)) return false;
    std::error_code error;
    return std::filesystem::remove(root_ / key, error) && !error;
}

}  // namespace placedb::storage
