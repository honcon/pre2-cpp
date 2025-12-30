#include "sqz_unpacker.h"
#include <fstream>
#include <stdexcept>
#include <unordered_map>

namespace sqz {

// ============================================================================
// Bit Readers
// ============================================================================

class LzwCodeWordReader {
    std::istream& stream;
    uint32_t buf24 = 0;
    int missing_bits = 0;
    
public:
    LzwCodeWordReader(std::istream& s) : stream(s) {
        uint8_t b1, b2, b3;
        stream.read(reinterpret_cast<char*>(&b1), 1);
        stream.read(reinterpret_cast<char*>(&b2), 1);
        stream.read(reinterpret_cast<char*>(&b3), 1);
        buf24 = (b1 << 16) | (b2 << 8) | b3;
    }
    
    int read_codeword(int nbit) {
        int cw = buf24 >> (24 - nbit);
        buf24 = (buf24 << nbit) & 0xFFFFFF;
        missing_bits += nbit;
        
        while (missing_bits >= 8 && stream.good()) {
            missing_bits -= 8;
            uint8_t b;
            if (stream.read(reinterpret_cast<char*>(&b), 1)) {
                buf24 |= (b << missing_bits);
            }
        }
        return cw;
    }
};

class BitReader {
    std::istream& stream;
    int bit = 8;
    uint8_t current_byte = 0;
    
public:
    BitReader(std::istream& s) : stream(s) {}
    
    bool is_eof() {
        return (bit == 8) && !stream.good();
    }
    
    bool read_bit(bool big_endian = false) {
        if (bit == 8) {
            stream.read(reinterpret_cast<char*>(&current_byte), 1);
            bit = 0;
        }
        int shift = big_endian ? (7 - bit) : bit;
        bool value = (current_byte & (1 << shift)) != 0;
        bit++;
        return value;
    }
};

class DietBitReader {
    std::istream& stream;
    int bit = 0;
    uint16_t current_word = 0;
    
public:
    DietBitReader(std::istream& s) : stream(s) {
        stream.read(reinterpret_cast<char*>(&current_word), 2);
    }
    
    bool read_bit() {
        bool value = (current_word & (1 << bit)) != 0;
        bit++;
        if (bit == 16) {
            stream.read(reinterpret_cast<char*>(&current_word), 2);
            bit = 0;
        }
        return value;
    }
    
    int read_3bit_value() {
        int value = 0;
        for (int i = 0; i < 3; i++) {
            value = (value << 1) | (read_bit() ? 1 : 0);
        }
        return value;
    }
    
    uint8_t read_next_byte() {
        uint8_t b;
        stream.read(reinterpret_cast<char*>(&b), 1);
        return b;
    }
};

// ============================================================================
// Huffman Tree Reader
// ============================================================================

class TtfHuffmanReader {
    std::vector<uint16_t> huffman_tree;
    BitReader bit_reader;
    
    bool is_parent_node(uint16_t node) {
        return (node & 0x8000) == 0;
    }
    
public:
    TtfHuffmanReader(std::istream& s) : bit_reader(s) {
        uint16_t tree_size_bytes;
        s.read(reinterpret_cast<char*>(&tree_size_bytes), 2);
        int tree_size = tree_size_bytes >> 1;
        
        huffman_tree.resize(tree_size);
        for (int i = 0; i < tree_size; i++) {
            uint16_t node;
            s.read(reinterpret_cast<char*>(&node), 2);
            huffman_tree[i] = is_parent_node(node) ? (node >> 1) : node;
        }
    }
    
    int read_codeword() {
        int node_idx = 0;
        while (!bit_reader.is_eof()) {
            bool choose_first = !bit_reader.read_bit(true);
            uint16_t current_node = huffman_tree[choose_first ? node_idx : node_idx + 1];
            if (is_parent_node(current_node)) {
                node_idx = current_node;
            } else {
                return current_node & 0x7FFF;
            }
        }
        return -1;
    }
};

// ============================================================================
// LZW Decompression
// ============================================================================

static void decode_lzw(std::istream& input, std::vector<uint8_t>& output, bool alt_lzw = false) {
    int code_clear = alt_lzw ? 0x101 : 0x100;
    int code_end = alt_lzw ? 0x100 : 0x101;
    const int dict_size_initial = 0x102;
    const int dict_limit = 0x1000;
    
    int nbit = 9;
    int dictsize = dict_size_initial;
    
    std::unordered_map<int, std::vector<uint8_t>> dict;
    for (int i = 0; i < 256; i++) {
        dict[i] = {static_cast<uint8_t>(i)};
    }
    
    LzwCodeWordReader cw_reader(input);
    
    int prev = code_clear;
    while (prev != code_end) {
        if (prev == code_clear) {
            nbit = 9;
            dictsize = dict_size_initial;
            dict.clear();
            for (int i = 0; i < 256; i++) {
                dict[i] = {static_cast<uint8_t>(i)};
            }
        }
        
        int cw = cw_reader.read_codeword(nbit);
        if (cw != code_end && cw != code_clear) {
            uint8_t newbyte;
            if (cw < dictsize) {
                newbyte = dict[cw][0];
            } else {
                if (prev == code_clear || dictsize >= dict_limit || cw != dictsize) {
                    throw std::runtime_error("Invalid LZW data");
                }
                newbyte = dict[prev][0];
            }
            
            if ((prev != code_clear) && (dictsize < dict_limit)) {
                std::vector<uint8_t> new_entry = dict[prev];
                new_entry.push_back(newbyte);
                dict[dictsize] = new_entry;
                dictsize++;
                
                int max_dict_size = 1 << nbit;
                if (dictsize == max_dict_size && nbit < 12) {
                    nbit++;
                }
            }
            
            const auto& out_data = dict[cw];
            output.insert(output.end(), out_data.begin(), out_data.end());
        }
        prev = cw;
    }
}

// ============================================================================
// Huffman RLE Decompression
// ============================================================================

static void decode_huffman_rle(std::istream& input, std::vector<uint8_t>& output) {
    TtfHuffmanReader huffman_reader(input);
    
    uint8_t last = 0;
    int cw;
    while ((cw = huffman_reader.read_codeword()) != -1) {
        int lo = cw & 0x00FF;
        int hi = cw & 0xFF00;
        
        if (hi == 0) {
            last = static_cast<uint8_t>(lo);
            output.push_back(last);
        } else {
            int count = 0;
            switch (lo) {
                case 0:
                    cw = huffman_reader.read_codeword();
                    if (cw == -1) return;
                    count = cw;
                    break;
                case 1: {
                    cw = huffman_reader.read_codeword();
                    if (cw == -1) return;
                    int count_hi = cw & 0xFF;
                    cw = huffman_reader.read_codeword();
                    if (cw == -1) return;
                    int count_lo = cw & 0xFF;
                    count = (count_hi << 8) | count_lo;
                    break;
                }
                default:
                    count = lo;
                    break;
            }
            for (int i = 0; i < count; i++) {
                output.push_back(last);
            }
        }
    }
}

// ============================================================================
// DIET Decompression
// ============================================================================

static uint8_t shift_left_add_bit(uint8_t b, bool bit) {
    return (b << 1) | (bit ? 1 : 0);
}

static uint8_t read_hi_byte_varlen(DietBitReader& reader) {
    uint8_t b = 0xFF;
    b = shift_left_add_bit(b, reader.read_bit());
    
    if (!reader.read_bit()) {
        uint8_t tmp = 1 << 1;
        for (int i = 0; i < 3; i++) {
            if (reader.read_bit()) break;
            b = shift_left_add_bit(b, reader.read_bit());
            tmp <<= 1;
        }
        b -= tmp;
    }
    return b;
}

static int read_repeat_count_varlen(DietBitReader& reader) {
    int count = 0;
    for (int i = 0; i < 4; i++) {
        count++;
        if (reader.read_bit()) return count;
    }
    
    if (reader.read_bit()) {
        return reader.read_bit() ? 6 : 5;
    } else {
        if (!reader.read_bit()) {
            return 7 + reader.read_3bit_value();
        } else {
            return 15 + reader.read_next_byte();
        }
    }
}

static void decode_diet(std::istream& input, std::vector<uint8_t>& output, size_t payload_size) {
    output.resize(payload_size);
    DietBitReader bit_reader(input);
    
    size_t idx = 0;
    while (idx < payload_size) {
        while (bit_reader.read_bit()) {
            output[idx++] = bit_reader.read_next_byte();
            if (idx >= payload_size) return;
        }
        
        bool bit = bit_reader.read_bit();
        uint8_t off_lo = bit_reader.read_next_byte();
        uint8_t off_hi;
        int repeat_count;
        
        if (!bit) {
            if (bit_reader.read_bit()) {
                off_hi = (0xF8 | bit_reader.read_3bit_value()) - 1;
            } else {
                off_hi = 0xFF;
                if (off_lo == 0xFF) return;
            }
            repeat_count = 2;
        } else {
            off_hi = read_hi_byte_varlen(bit_reader);
            repeat_count = 2 + read_repeat_count_varlen(bit_reader);
        }
        
        int16_t offset = static_cast<int16_t>((off_hi << 8) | off_lo);
        size_t source_idx = idx + offset;
        
        for (int i = 0; i < repeat_count && idx < payload_size; i++) {
            output[idx++] = output[source_idx++];
        }
    }
}

// ============================================================================
// Main Unpack Function
// ============================================================================

std::vector<uint8_t> unpack(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary);
    if (!file) {
        throw std::runtime_error("Cannot open file: " + filename);
    }
    
    // Check for DIET signature
    uint16_t signature;
    file.read(reinterpret_cast<char*>(&signature), 2);
    file.seekg(0);
    
    if (signature == 0x4CB4) {
        // DIET compressed
        uint8_t diet_sig[9];
        file.read(reinterpret_cast<char*>(diet_sig), 9);
        
        file.seekg(1, std::ios::cur); // skip b09
        file.seekg(4, std::ios::cur); // skip checksum
        
        uint8_t size_hi_byte;
        file.read(reinterpret_cast<char*>(&size_hi_byte), 1);
        int payload_size_hi = (size_hi_byte >> 2) & 0x1F;
        
        uint16_t payload_size_lo;
        file.read(reinterpret_cast<char*>(&payload_size_lo), 2);
        
        size_t payload_size = (payload_size_hi << 16) | payload_size_lo;
        
        std::vector<uint8_t> output;
        decode_diet(file, output, payload_size);
        return output;
    } else {
        // TTF format - LZW or Huffman
        uint8_t first_byte;
        file.read(reinterpret_cast<char*>(&first_byte), 1);
        int payload_size_hi = first_byte & 0x0F;
        
        uint8_t type;
        file.read(reinterpret_cast<char*>(&type), 1);
        
        uint16_t payload_size_lo;
        file.read(reinterpret_cast<char*>(&payload_size_lo), 2);
        
        size_t payload_size = (payload_size_hi << 16) | payload_size_lo;
        
        std::vector<uint8_t> output;
        output.reserve(payload_size);
        
        if (type == 0x10) {
            decode_lzw(file, output);
        } else {
            decode_huffman_rle(file, output);
        }
        
        return output;
    }
}

} // namespace sqz
