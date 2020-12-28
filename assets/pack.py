from bitstring import BitArray

words_csv = open("./words_plain.csv")
words_packed = open("./words_packed.bin", "wb")

bit_data = BitArray()

for line in words_csv:
    split = line.strip().split(",")

    targets = [int(x) for x in split[1:4]]
    full_word = split[0]
    word_list = split[4].split(" ")

    word_list.remove(full_word)

    #targets
    bit_data.append(BitArray(uint=targets[0], length=6))
    bit_data.append(BitArray(uint=targets[1], length=6))
    bit_data.append(BitArray(uint=targets[2], length=6))

    bit_data.append(BitArray().join(BitArray(uint=ord(x) - ord('a'), length=5) for x in full_word))

    word_index = []

    for word in word_list:
        word_index += [full_word.index(c) for c in word] + [7]

    packed_data = BitArray().join(BitArray(uint=x, length=3) for x in word_index)

    bit_data.append(BitArray(uint=len(word_list), length=6))
    bit_data.append(packed_data)

words_packed.write(bit_data.tobytes())