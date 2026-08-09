#include "auth/secret.h"
#include "storage/avatar_store.h"

#include <array>
#include <cassert>
#include <filesystem>
#include <vector>

int main() {
    assert(placedb::auth::InitializeCryptoOnce());
    const auto root = std::filesystem::temp_directory_path() /
                      ("placedb-avatar-test-" +
                       placedb::auth::HashToken(placedb::auth::MintToken()).substr(0, 16));
    placedb::storage::LocalAvatarStore store(root);

    const std::array<unsigned char, 8> invalid{};
    assert(!store.Put(invalid).has_value());

    const std::array<unsigned char, 12> png{
        0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 1, 2, 3, 4};
    const auto stored = store.Put(png);
    assert(stored.has_value());
    assert(stored->key.size() == 44);
    assert(stored->content_type == "image/png");
    const auto loaded = store.Get(stored->key);
    assert(loaded.has_value());
    assert(std::vector<unsigned char>(png.begin(), png.end()) == *loaded);

    assert(!store.Get("../../etc/passwd").has_value());
    assert(!store.Remove("../../etc/passwd"));
    assert(store.Remove(stored->key));
    assert(!store.Get(stored->key).has_value());

    std::error_code error;
    std::filesystem::remove_all(root, error);
}
