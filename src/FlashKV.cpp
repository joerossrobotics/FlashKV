#include "FlashKV/FlashKV.h"

namespace FlashKV
{
    FlashKV::FlashKV(FlashWriteFunc flashWriteFunc,
                     FlashReadFunc flashReadFunc,
                     FlashEraseFunc flashEraseFunc,
                     size_t flashPageSize,
                     size_t flashSectorSize,
                     size_t flashAddress,
                     size_t flashSize)
        : _flashWriteFunc(flashWriteFunc),
          _flashReadFunc(flashReadFunc),
          _flashEraseFunc(flashEraseFunc),
          _flashPageSize(flashPageSize),
          _flashSectorSize(flashSectorSize),
          _flashAddress(flashAddress),
          _flashSize(flashSize)
    {
    }

    FlashKV::~FlashKV()
    {
        saveStore();
    }

    int FlashKV::loadStore()
    {
        // Read the first few bytes from Flash memory.
        std::array<uint8_t, SIGNATURE_SIZE> signature;
        if (!_flashReadFunc(_flashAddress, signature.data(), SIGNATURE_SIZE))
            return false;

        // Check if the signature matches.
        if (signature == FLASHKV_SIGNATURE)
        {
            // The Flash memory contains a valid FlashKV Store
            // TODO: Read the store from Flash memory.

            _storeLoaded = true;
            return 0;
        }
        else
        {
            // The Flash memory does not contain a valid FlashKV store, Erase the memory here.
            if (!_flashEraseFunc(_flashAddress, _flashSize))
            {
                _storeLoaded = false;
                return -1;
            }

            // Write the signature to the first page of Flash memory.
            std::vector<uint8_t> buffer(_flashPageSize, 0);
            std::copy(FLASHKV_SIGNATURE.begin(), FLASHKV_SIGNATURE.end(), buffer.begin());
            if (!_flashWriteFunc(_flashAddress, buffer.data(), _flashPageSize))
            {
                _storeLoaded = false;
                return -1;
            }

            _storeLoaded = true;
            return 1;
        }
        return -1;
    }

    bool FlashKV::saveStore()
    {
        // If The Store Hasn't Been Loaded, Return False.
        if (!_storeLoaded)
            return false;

        // Erase Entire Flash Memory.
        if (!_flashEraseFunc(_flashAddress, _flashSize))
            return false;

        // Create a buffer to hold the serialized store.
        std::vector<uint8_t> buffer;

        // Add the signature to the buffer
        buffer.insert(buffer.end(), FLASHKV_SIGNATURE.begin(), FLASHKV_SIGNATURE.end());

        // Add each key-value pair to the buffer.
        for (const auto &kv : _keyValueStore)
        {
            // Serialize the key
            size_t keySize = kv.first.size();
            buffer.insert(buffer.end(), reinterpret_cast<uint8_t *>(&keySize), reinterpret_cast<uint8_t *>(&keySize) + sizeof(size_t));
            buffer.insert(buffer.end(), kv.first.begin(), kv.first.end());

            // Serialize the value
            size_t valueSize = kv.second.size();
            buffer.insert(buffer.end(), reinterpret_cast<uint8_t *>(&valueSize), reinterpret_cast<uint8_t *>(&valueSize) + sizeof(size_t));
            buffer.insert(buffer.end(), kv.second.begin(), kv.second.end());
        }

        // Pad the buffer with zeros to make it a multiple of the page size
        while (buffer.size() % _flashPageSize != 0)
            buffer.push_back(0);

        // Write the buffer to Flash memory
        if (!_flashWriteFunc(_flashAddress, buffer.data(), buffer.size()))
            return false;

        return true;
    }

    bool FlashKV::writeKey(std::string key, std::vector<uint8_t> value)
    {
        if (!_storeLoaded)
            return false;

        _keyValueStore[key] = value;
        return true;
    }

    std::optional<std::vector<uint8_t>> FlashKV::readKey(std::string key)
    {
        if (!_storeLoaded)
            return std::nullopt;

        auto it = _keyValueStore.find(key);
        if (it != _keyValueStore.end())
            return it->second;

        return std::nullopt;
    }

    bool FlashKV::eraseKey(std::string key)
    {
        if (!_storeLoaded)
            return false;

        return _keyValueStore.erase(key) > 0;
    }

} // namespace FlashKV
