/*
 * FlashKV - A Lightweight, Hardware-Agnostic Key-Value Map for Flash Memory
 *
 * Copyright (C) 2023 Joe Inman
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the MIT License as published by
 * the Open Source Initiative.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * MIT License for more details.
 *
 * You should have received a copy of the MIT License along with this program.
 * If not, see <https://opensource.org/licenses/MIT>.
 *
 * Author: Joe Inman
 * Email: joe.inman8@gmail.com
 * Version: 1.0
 *
 * Description:
 * FlashKV is designed to provide a straightforward and customizable
 * interface for key-value storage in flash memory. This library
 * allows for easy reading, writing, and erasing of key-value pairs.
 *
 */

#include "../include/FlashKV/FlashKV.h"

namespace FlashKV
{
    FlashKV::FlashKV(FlashWriteFunction flashWriteFunction,
                     FlashReadFunction flashReadFunc,
                     FlashEraseFunction flashEraseFunction,
                     size_t flashPageSize,
                     size_t flashSectorSize,
                     size_t flashAddress,
                     size_t flashSize)
        : flashWriteFunction(flashWriteFunction),
          flashReadFunction(flashReadFunc),
          flashEraseFunction(flashEraseFunction),
          flashPageSize(flashPageSize),
          flashSectorSize(flashSectorSize),
          flashAddress(flashAddress),
          flashSize(flashSize)
    {
    }

    FlashKV::~FlashKV() {}

    int FlashKV::loadMap()
    {
        if (verifySignature())
        {
            serialisedSize = FLASHKV_SIGNATURE_SIZE;
            while (1)
            {
                // Read The Size Of The Key
                uint16_t keySize;
                flashReadFunction(flashAddress + serialisedSize, reinterpret_cast<uint8_t *>(&keySize), sizeof(uint16_t));

                // If The Key Size Is 0, We Have Reached The End Of The Map
                if (keySize == 0)
                    break;
                serialisedSize += sizeof(uint16_t);

                // Read The Key
                std::string key;
                key.resize(keySize);
                flashReadFunction(flashAddress + serialisedSize, reinterpret_cast<uint8_t *>(&key[0]), keySize);
                serialisedSize += keySize;

                // Read The Size Of The Value
                uint16_t valueSize;
                flashReadFunction(flashAddress + serialisedSize, reinterpret_cast<uint8_t *>(&valueSize), sizeof(uint16_t));
                serialisedSize += sizeof(uint16_t);

                // Read The Value
                std::vector<uint8_t> value;
                value.resize(valueSize);
                flashReadFunction(flashAddress + serialisedSize, reinterpret_cast<uint8_t *>(&value[0]), valueSize);
                serialisedSize += valueSize;

                // Add Key-Value Pair To The Map
                keyValueMap[key] = value;
            }

            // Load Was Successful
            mapLoaded = true;
            return 0;
        }
        else
        {
            // Flash Doesn't Contain A Valid FlashKV Map
            mapLoaded = true;
            return 1;
        }
        
        return -1;
    }

    bool FlashKV::saveMap()
    {
        if (mapLoaded && flashEraseFunction(flashAddress, flashSize))
        {
            std::vector<uint8_t> buffer;
            buffer.reserve(serialisedSize);
            buffer.insert(buffer.end(), std::begin(FLASHKV_SIGNATURE), std::end(FLASHKV_SIGNATURE));

            for (const auto &[key, value] : keyValueMap)
            {
                auto kvBytes = serialiseKeyValuePair(key, value);
                buffer.insert(buffer.end(), kvBytes.begin(), kvBytes.end());
            }

            while (buffer.size() % flashPageSize != 0)
                buffer.push_back(0);

            return flashWriteFunction(flashAddress, buffer.data(), buffer.size());
        }

        return false;
    }

    bool FlashKV::writeKey(std::string key, std::vector<uint8_t> value)
    {
        if (mapLoaded)
        {
            if (serialisedSize + sizeof(uint16_t) + key.size() + sizeof(uint16_t) + value.size() <= flashSize)
            {
                keyValueMap[key] = value;
                serialisedSize += sizeof(uint16_t) + key.size() + sizeof(uint16_t) + value.size();
                return true;
            }
        }

        return false;
    }

    std::optional<std::vector<uint8_t>> FlashKV::readKey(std::string key)
    {
        if (mapLoaded)
        {
            auto it = keyValueMap.find(key);
            if (it != keyValueMap.end())
                return it->second;
        }

        return std::nullopt;
    }

    bool FlashKV::eraseKey(std::string key)
    {
        if (mapLoaded)
        {
            auto it = keyValueMap.find(key);
            if (it != keyValueMap.end())
            {
                serialisedSize -= sizeof(uint16_t) + key.size() + sizeof(uint16_t) + it->second.size();
                keyValueMap.erase(it);
                return true;
            }
        }

        return false;
    }

    std::vector<std::string> FlashKV::getAllKeys()
    {
        std::vector<std::string> keys;
        for (const auto &[key, value] : keyValueMap)
            keys.push_back(key);
        return keys;
    }

    bool FlashKV::verifySignature()
    {
        uint8_t signature[FLASHKV_SIGNATURE_SIZE];
        if (!flashReadFunction(flashAddress, signature, FLASHKV_SIGNATURE_SIZE))
            return false;

        return std::memcmp(signature, FLASHKV_SIGNATURE, FLASHKV_SIGNATURE_SIZE) == 0;
    }

    std::vector<uint8_t> FlashKV::serialiseKeyValuePair(const std::string &key, const std::vector<uint8_t> &value)
    {
        std::vector<uint8_t> buffer;
        uint16_t keySize = key.size();
        uint16_t valueSize = value.size();

        buffer.insert(buffer.end(), reinterpret_cast<uint8_t *>(&keySize), reinterpret_cast<uint8_t *>(&keySize) + sizeof(uint16_t));
        buffer.insert(buffer.end(), key.begin(), key.end());

        buffer.insert(buffer.end(), reinterpret_cast<uint8_t *>(&valueSize), reinterpret_cast<uint8_t *>(&valueSize) + sizeof(uint16_t));
        buffer.insert(buffer.end(), value.begin(), value.end());

        return buffer;
    }

} // namespace FlashKV
