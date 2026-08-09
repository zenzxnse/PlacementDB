#ifndef PLACEDB_STORAGE_AVATAR_STORE_H
#define PLACEDB_STORAGE_AVATAR_STORE_H

#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace placedb::storage {

struct StoredAvatar {
    std::string key;
    std::string content_type;
};

class AvatarStore {
  public:
    virtual ~AvatarStore() = default;
    virtual std::optional<StoredAvatar> Put(
        std::span<const unsigned char> bytes) = 0;
    virtual std::optional<std::vector<unsigned char>> Get(
        const std::string& key) const = 0;
    virtual bool Remove(const std::string& key) = 0;
};

class LocalAvatarStore final : public AvatarStore {
  public:
    explicit LocalAvatarStore(std::filesystem::path root);
    std::optional<StoredAvatar> Put(
        std::span<const unsigned char> bytes) override;
    std::optional<std::vector<unsigned char>> Get(
        const std::string& key) const override;
    bool Remove(const std::string& key) override;

  private:
    std::filesystem::path root_;
};

}  // namespace placedb::storage

#endif
