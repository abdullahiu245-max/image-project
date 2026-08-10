#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cmath>
#include <algorithm>
#include <iomanip>
#include <cstdint>
#include <sstream>
#include <limits>

#ifdef USE_OPENCV
#include <opencv2/opencv.hpp>
#endif

using namespace std;

/*
=============================================================
       IMAGE COMPRESSION USING EZW TECHNIQUE
       Embedded Zerotree Wavelet (EZW)
       C++ Implementation

       Input  : PGM / JPG / JPEG / PNG image
                (JPG/JPEG/PNG require compiling with
                 USE_OPENCV, see build instructions at the
                 bottom of this file)
       Output : EZW compressed file
                Reconstructed image (same format as chosen
                output filename: .pgm, .png, .jpg)

       Wavelet: Haar Wavelet

       Build without OpenCV (PGM only):
           g++ -O2 -std=c++17 ezw_compression.cpp -o ezw

       Build with OpenCV (adds JPG/PNG support):
           g++ -O2 -std=c++17 -DUSE_OPENCV ezw_compression.cpp \
               -o ezw `pkg-config --cflags --libs opencv4`
=============================================================
*/

struct Image
{
    int width = 0;
    int height = 0;
    int maxValue = 255;
    vector<double> pixels;
};

struct EZWHeader
{
    int paddedWidth;
    int paddedHeight;
    int origWidth;
    int origHeight;
    int levels;
    int passes;
    double threshold;
    int bitCount;
};

/*
=============================================================
                  UTILITY FUNCTIONS
=============================================================
*/

string nextToken(ifstream& file)
{
    string token;

    while (file >> token)
    {
        if (!token.empty() && token[0] == '#')
        {
            string dummy;
            getline(file, dummy);
            continue;
        }

        return token;
    }

    return "";
}

string toLower(const string& s)
{
    string result = s;
    transform(result.begin(), result.end(), result.begin(),
              [](unsigned char c) { return tolower(c); });
    return result;
}

string getExtension(const string& filename)
{
    size_t dot = filename.find_last_of('.');

    if (dot == string::npos)
        return "";

    return toLower(filename.substr(dot + 1));
}

/*
=============================================================
                  READ PGM IMAGE
=============================================================
*/

bool readPGM(const string& filename, Image& image)
{
    ifstream file(filename, ios::binary);

    if (!file)
    {
        cerr << "Error: Cannot open input image: "
             << filename << endl;
        return false;
    }

    string magic = nextToken(file);

    if (magic != "P2" && magic != "P5")
    {
        cerr << "Error: Only P2 and P5 PGM images are supported."
             << endl;
        return false;
    }

    image.width = stoi(nextToken(file));
    image.height = stoi(nextToken(file));
    image.maxValue = stoi(nextToken(file));

    if (image.width <= 0 || image.height <= 0)
    {
        cerr << "Invalid image dimensions." << endl;
        return false;
    }

    image.pixels.resize(image.width * image.height);

    if (magic == "P2")
    {
        for (int i = 0; i < image.width * image.height; i++)
        {
            string value = nextToken(file);

            if (value.empty())
            {
                cerr << "Unexpected end of PGM file." << endl;
                return false;
            }

            image.pixels[i] = stod(value);
        }
    }
    else
    {
        file.get();

        if (image.maxValue <= 255)
        {
            for (int i = 0; i < image.width * image.height; i++)
            {
                unsigned char value;

                file.read(reinterpret_cast<char*>(&value), 1);

                if (!file)
                {
                    cerr << "Error reading image data." << endl;
                    return false;
                }

                image.pixels[i] = value;
            }
        }
        else
        {
            for (int i = 0; i < image.width * image.height; i++)
            {
                unsigned char high, low;

                file.read(reinterpret_cast<char*>(&high), 1);
                file.read(reinterpret_cast<char*>(&low), 1);

                image.pixels[i] =
                    (static_cast<int>(high) << 8) | low;
            }
        }
    }

    return true;
}

/*
=============================================================
                  WRITE PGM IMAGE
=============================================================
*/

bool writePGM(const string& filename, const Image& image)
{
    ofstream file(filename, ios::binary);

    if (!file)
    {
        cerr << "Error: Cannot create output image." << endl;
        return false;
    }

    file << "P5\n";
    file << image.width << " " << image.height << "\n";
    file << "255\n";

    for (double value : image.pixels)
    {
        int pixel = static_cast<int>(round(value));

        pixel = max(0, min(255, pixel));

        unsigned char byte =
            static_cast<unsigned char>(pixel);

        file.write(reinterpret_cast<char*>(&byte), 1);
    }

    return true;
}

/*
=============================================================
      GENERIC IMAGE READ / WRITE (PGM always available,
      JPG/PNG only when compiled with -DUSE_OPENCV)
=============================================================
*/

bool readImage(const string& filename, Image& image)
{
    string ext = getExtension(filename);

    if (ext == "pgm")
    {
        return readPGM(filename, image);
    }

#ifdef USE_OPENCV
    cv::Mat loaded = cv::imread(filename, cv::IMREAD_GRAYSCALE);

    if (loaded.empty())
    {
        cerr << "Error: Cannot open input image: "
             << filename << endl;
        return false;
    }

    image.width = loaded.cols;
    image.height = loaded.rows;
    image.maxValue = 255;
    image.pixels.resize(image.width * image.height);

    for (int y = 0; y < image.height; y++)
    {
        for (int x = 0; x < image.width; x++)
        {
            image.pixels[y * image.width + x] =
                static_cast<double>(loaded.at<unsigned char>(y, x));
        }
    }

    return true;
#else
    cerr << "Error: \"" << filename
         << "\" is not a .pgm file.\n"
         << "JPG/PNG support requires compiling with -DUSE_OPENCV "
         << "(see build instructions at the top of this file).\n";
    return false;
#endif
}

bool writeImage(const string& filename, const Image& image)
{
    string ext = getExtension(filename);

    if (ext == "pgm" || ext.empty())
    {
        return writePGM(filename, image);
    }

#ifdef USE_OPENCV
    cv::Mat out(image.height, image.width, CV_8UC1);

    for (int y = 0; y < image.height; y++)
    {
        for (int x = 0; x < image.width; x++)
        {
            int pixel = static_cast<int>(round(image.pixels[y * image.width + x]));
            pixel = max(0, min(255, pixel));
            out.at<unsigned char>(y, x) = static_cast<unsigned char>(pixel);
        }
    }

    if (!cv::imwrite(filename, out))
    {
        cerr << "Error: Could not write output image: "
             << filename << endl;
        return false;
    }

    return true;
#else
    cerr << "Error: \"" << filename
         << "\" is not a .pgm file.\n"
         << "JPG/PNG support requires compiling with -DUSE_OPENCV.\n";
    return false;
#endif
}

/*
=============================================================
       DIMENSION HANDLING: PAD TO POWER OF TWO / CROP BACK

       The wavelet transform below needs width and height
       that are each a power of two. Real photos almost never
       are, so instead of rejecting the image we pad it (by
       replicating the edge pixels, which keeps the padded
       border smooth and doesn't inject artificial high
       frequency energy into the wavelet transform) and crop
       the padded region back off after reconstruction.
=============================================================
*/

bool isPowerOfTwo(int n)
{
    return n > 0 && (n & (n - 1)) == 0;
}

int nextPowerOfTwo(int n)
{
    int p = 1;

    while (p < n)
        p *= 2;

    return p;
}

int calculateLevels(int width, int height)
{
    int levels = 0;

    int size = min(width, height);

    while (size >= 2 && size % 2 == 0)
    {
        levels++;
        size /= 2;
    }

    return levels;
}

Image padImage(const Image& src, int paddedWidth, int paddedHeight)
{
    Image padded;

    padded.width = paddedWidth;
    padded.height = paddedHeight;
    padded.maxValue = src.maxValue;
    padded.pixels.resize(paddedWidth * paddedHeight);

    for (int y = 0; y < paddedHeight; y++)
    {
        int srcY = min(y, src.height - 1);

        for (int x = 0; x < paddedWidth; x++)
        {
            int srcX = min(x, src.width - 1);

            padded.pixels[y * paddedWidth + x] =
                src.pixels[srcY * src.width + srcX];
        }
    }

    return padded;
}

Image cropImage(const Image& src, int origWidth, int origHeight)
{
    Image cropped;

    cropped.width = origWidth;
    cropped.height = origHeight;
    cropped.maxValue = src.maxValue;
    cropped.pixels.resize(origWidth * origHeight);

    for (int y = 0; y < origHeight; y++)
    {
        for (int x = 0; x < origWidth; x++)
        {
            cropped.pixels[y * origWidth + x] =
                src.pixels[y * src.width + x];
        }
    }

    return cropped;
}

/*
=============================================================
                  HAAR WAVELET TRANSFORM
=============================================================
*/

void haarForward(vector<double>& data,
                 int width,
                 int height,
                 int levels)
{
    vector<double> temp(width * height);

    int currentWidth = width;
    int currentHeight = height;

    for (int level = 0; level < levels; level++)
    {
        // Row transform
        for (int y = 0; y < currentHeight; y++)
        {
            for (int x = 0; x < currentWidth; x += 2)
            {
                double a = data[y * width + x];
                double b = data[y * width + x + 1];

                temp[y * width + x / 2] =
                    (a + b) / sqrt(2.0);

                temp[y * width + currentWidth / 2 + x / 2] =
                    (a - b) / sqrt(2.0);
            }
        }

        // Column transform
        for (int x = 0; x < currentWidth; x++)
        {
            for (int y = 0; y < currentHeight; y += 2)
            {
                double a = temp[y * width + x];
                double b = temp[(y + 1) * width + x];

                data[(y / 2) * width + x] =
                    (a + b) / sqrt(2.0);

                data[(currentHeight / 2 + y / 2) * width + x] =
                    (a - b) / sqrt(2.0);
            }
        }

        currentWidth /= 2;
        currentHeight /= 2;
    }
}

/*
=============================================================
                  INVERSE HAAR TRANSFORM
=============================================================
*/

void haarInverse(vector<double>& data,
                 int width,
                 int height,
                 int levels)
{
    vector<double> temp(width * height);

    int currentWidth = width >> (levels - 1);
    int currentHeight = height >> (levels - 1);

    for (int level = levels - 1; level >= 0; level--)
    {
        // Inverse column transform
        for (int x = 0; x < currentWidth; x++)
        {
            for (int y = 0; y < currentHeight / 2; y++)
            {
                double low =
                    data[y * width + x];

                double high =
                    data[(currentHeight / 2 + y) * width + x];

                temp[(2 * y) * width + x] =
                    (low + high) / sqrt(2.0);

                temp[(2 * y + 1) * width + x] =
                    (low - high) / sqrt(2.0);
            }
        }

        // Inverse row transform
        for (int y = 0; y < currentHeight; y++)
        {
            for (int x = 0; x < currentWidth / 2; x++)
            {
                double low =
                    temp[y * width + x];

                double high =
                    temp[y * width + currentWidth / 2 + x];

                data[y * width + 2 * x] =
                    (low + high) / sqrt(2.0);

                data[y * width + 2 * x + 1] =
                    (low - high) / sqrt(2.0);
            }
        }

        currentWidth *= 2;
        currentHeight *= 2;
    }
}

/*
=============================================================
                  BIT WRITER
=============================================================
*/

class BitWriter
{
private:

    vector<unsigned char> bytes;
    unsigned char currentByte = 0;
    int bitPosition = 0;

public:

    void writeBit(int bit)
    {
        currentByte <<= 1;

        if (bit)
            currentByte |= 1;

        bitPosition++;

        if (bitPosition == 8)
        {
            bytes.push_back(currentByte);

            currentByte = 0;
            bitPosition = 0;
        }
    }

    void writeBits(uint32_t value, int count)
    {
        for (int i = count - 1; i >= 0; i--)
        {
            writeBit((value >> i) & 1);
        }
    }

    void flush()
    {
        if (bitPosition > 0)
        {
            currentByte <<= (8 - bitPosition);

            bytes.push_back(currentByte);

            currentByte = 0;
            bitPosition = 0;
        }
    }

    const vector<unsigned char>& getBytes()
    {
        return bytes;
    }

    int getBitCount() const
    {
        return static_cast<int>(bytes.size() * 8);
    }
};

/*
=============================================================
                  BIT READER
=============================================================
*/

class BitReader
{
private:

    const vector<unsigned char>& bytes;

    int bytePosition = 0;
    int bitPosition = 0;

public:

    BitReader(const vector<unsigned char>& b)
        : bytes(b)
    {
    }

    int readBit()
    {
        if (bytePosition >= static_cast<int>(bytes.size()))
            return 0;

        int bit =
            (bytes[bytePosition] >>
             (7 - bitPosition)) & 1;

        bitPosition++;

        if (bitPosition == 8)
        {
            bitPosition = 0;
            bytePosition++;
        }

        return bit;
    }

    uint32_t readBits(int count)
    {
        uint32_t value = 0;

        for (int i = 0; i < count; i++)
        {
            value <<= 1;
            value |= readBit();
        }

        return value;
    }
};

/*
=============================================================
                  EZW SYMBOLS
=============================================================

POSITIVE = 00   significant, positive
NEGATIVE = 01   significant, negative
ZERO     = 10   insignificant, but at least one descendant
                may still be significant later (an
                "isolated zero" in EZW terminology)
ZEROTREE = 11   insignificant AND every descendant in the
                wavelet tree is also insignificant at this
                threshold, so the whole subtree is pruned
                and none of its coefficients are coded again
                until a later (lower-threshold) pass
=============================================================
*/

enum EZWSymbol
{
    POSITIVE = 0,
    NEGATIVE = 1,
    ZERO = 2,
    ZEROTREE = 3
};

/*
=============================================================
             WAVELET ZEROTREE STRUCTURE

    Builds the parent/child relationships between wavelet
    coefficients so the encoder/decoder can walk the tree
    and prune whole insignificant subtrees with a single
    ZEROTREE symbol, which is the entire point of EZW.

    Layout produced by haarForward()/haarInverse() above,
    for a decomposition with `levels` levels: the coarsest
    LL band sits in the top-left corner. Surrounding it,
    from coarsest to finest, are HL (top-right), LH
    (bottom-left) and HH (bottom-right) orientation bands
    at each level, each twice the size of the previous one.

    Parent/child rules (standard EZW quad-tree):
      - Each coarsest-level LL coefficient has 3 children:
        the coefficient at the same (x, y) position in the
        coarsest HL, LH and HH bands.
      - Each coefficient in an HL/LH/LH band (levels-1 down
        to level 1) has 4 children: the 2x2 block at the
        corresponding position in the same-orientation band
        one level finer.
      - Coefficients in the finest-level HL/LH/HH bands are
        leaves (no children).
=============================================================
*/

struct WaveletTree
{
    int width = 0;
    int height = 0;
    int levels = 0;

    vector<vector<int>> children;
    vector<int> scanOrder;

    int index(int x, int y) const
    {
        return y * width + x;
    }

    void link(int parent, int child)
    {
        children[parent].push_back(child);
    }

    void build(int w, int h, int lv)
    {
        width = w;
        height = h;
        levels = lv;

        children.assign(width * height, {});
        scanOrder.clear();
        scanOrder.reserve(width * height);

        int llW = width >> levels;
        int llH = height >> levels;

        // Root LL band comes first in the scan order.
        for (int y = 0; y < llH; y++)
            for (int x = 0; x < llW; x++)
                scanOrder.push_back(index(x, y));

        // Walk from the coarsest level to the finest,
        // adding HL, LH, HH bands and linking each one to
        // its parent band.
        for (int i = levels - 1; i >= 0; i--)
        {
            int qw = width >> (i + 1);
            int qh = height >> (i + 1);

            int hlX = qw, hlY = 0;
            int lhX = 0, lhY = qh;
            int hhX = qw, hhY = qh;

            for (int y = 0; y < qh; y++)
                for (int x = 0; x < qw; x++)
                    scanOrder.push_back(index(hlX + x, hlY + y));

            for (int y = 0; y < qh; y++)
                for (int x = 0; x < qw; x++)
                    scanOrder.push_back(index(lhX + x, lhY + y));

            for (int y = 0; y < qh; y++)
                for (int x = 0; x < qw; x++)
                    scanOrder.push_back(index(hhX + x, hhY + y));

            if (i == levels - 1)
            {
                // Parent is the root LL coefficient at the
                // same (x, y) — llW == qw and llH == qh here.
                for (int y = 0; y < qh; y++)
                {
                    for (int x = 0; x < qw; x++)
                    {
                        int p = index(x, y);

                        link(p, index(hlX + x, hlY + y));
                        link(p, index(lhX + x, lhY + y));
                        link(p, index(hhX + x, hhY + y));
                    }
                }
            }
            else
            {
                // Parent is the same-orientation coefficient
                // one level coarser, at (x/2, y/2).
                int pqw = width >> (i + 2);
                int pqh = height >> (i + 2);

                int pHlX = pqw, pHlY = 0;
                int pLhX = 0, pLhY = pqh;
                int pHhX = pqw, pHhY = pqh;

                for (int y = 0; y < qh; y++)
                {
                    for (int x = 0; x < qw; x++)
                    {
                        int px = x / 2;
                        int py = y / 2;

                        link(index(pHlX + px, pHlY + py), index(hlX + x, hlY + y));
                        link(index(pLhX + px, pLhY + py), index(lhX + x, lhY + y));
                        link(index(pHhX + px, pHhY + py), index(hhX + x, hhY + y));
                    }
                }
            }
        }
    }

    // True if `idx` and every coefficient beneath it in the
    // tree are insignificant at `threshold`.
    bool allInsignificant(int idx,
                          double threshold,
                          const vector<double>& coefficients,
                          const vector<char>& significant) const
    {
        if (significant[idx])
            return false;

        if (fabs(coefficients[idx]) >= threshold)
            return false;

        for (int c : children[idx])
        {
            if (!allInsignificant(c, threshold, coefficients, significant))
                return false;
        }

        return true;
    }

    void markSubtreeSkip(int idx, vector<char>& skip) const
    {
        for (int c : children[idx])
        {
            skip[c] = true;
            markSubtreeSkip(c, skip);
        }
    }
};

/*
=============================================================
                EZW ENCODER
=============================================================
*/

class EZWEncoder
{
private:

    vector<double> coefficients;

    int width;
    int height;

    double threshold;

    vector<char> significant;

    // Coefficients found significant so far, in the order
    // they were found. Refined every pass thereafter.
    vector<int> subordinateIndices;

    // Running reconstructed magnitude for each significant
    // coefficient (signed). Mirrors exactly what the decoder
    // computes from the bits it reads, so the encoder can
    // test against it instead of against a stale threshold.
    vector<double> reconValue;

    WaveletTree tree;

public:

    EZWEncoder(const vector<double>& coeffs,
               int w,
               int h,
               int levels)
    {
        coefficients = coeffs;

        width = w;
        height = h;

        threshold = 1.0;

        significant.assign(width * height, 0);
        reconValue.assign(width * height, 0.0);

        tree.build(width, height, levels);
    }

    double findInitialThreshold()
    {
        double maxValue = 0;

        for (double value : coefficients)
        {
            maxValue =
                max(maxValue, fabs(value));
        }

        if (maxValue == 0)
            return 1;

        double power = 1;

        while (power * 2 <= maxValue)
            power *= 2;

        return power;
    }

    void encode(BitWriter& writer,
                int passes = 8)
    {
        threshold = findInitialThreshold();

        subordinateIndices.clear();

        vector<char> skip(width * height, 0);

        for (int pass = 0; pass < passes; pass++)
        {
            /*
            -------------------------------------------------
                    DOMINANT (SIGNIFICANCE) PASS
                    Walks the wavelet tree coarse-to-fine,
                    pruning whole insignificant subtrees
                    with ZEROTREE instead of coding every
                    coefficient individually.
            -------------------------------------------------
            */

            fill(skip.begin(), skip.end(), 0);

            for (int index : tree.scanOrder)
            {
                if (significant[index] || skip[index])
                    continue;

                double value = fabs(coefficients[index]);

                if (value >= threshold)
                {
                    if (coefficients[index] >= 0)
                        writer.writeBits(POSITIVE, 2);
                    else
                        writer.writeBits(NEGATIVE, 2);

                    significant[index] = 1;

                    // Initial reconstruction estimate: the
                    // coefficient is known to lie in
                    // [threshold, 2*threshold), so start at
                    // the low end of that interval and
                    // narrow it in the refinement pass below.
                    reconValue[index] =
                        (coefficients[index] >= 0) ? threshold : -threshold;

                    subordinateIndices.push_back(index);
                }
                else if (tree.children[index].empty())
                {
                    // Leaf node — nothing to prune.
                    writer.writeBits(ZERO, 2);
                }
                else
                {
                    bool prunable = true;

                    for (int c : tree.children[index])
                    {
                        if (!tree.allInsignificant(c, threshold, coefficients, significant))
                        {
                            prunable = false;
                            break;
                        }
                    }

                    if (prunable)
                    {
                        writer.writeBits(ZEROTREE, 2);
                        tree.markSubtreeSkip(index, skip);
                    }
                    else
                    {
                        writer.writeBits(ZERO, 2);
                    }
                }
            }

            /*
            -------------------------------------------------
                    SUBORDINATE (REFINEMENT) PASS
                    Every coefficient found significant in
                    this pass or an earlier one gets one more
                    bit of precision: does the true value lie
                    in the upper half of the current
                    uncertainty interval? If so, narrow the
                    running reconstruction toward it. Both
                    encoder and decoder track the same
                    reconValue, so this always stays in sync.
            -------------------------------------------------
            */

            for (int index : subordinateIndices)
            {
                double trueMagnitude = fabs(coefficients[index]);
                double reconMagnitude = fabs(reconValue[index]);

                int bit =
                    (trueMagnitude >= reconMagnitude + threshold) ? 1 : 0;

                writer.writeBit(bit);

                if (bit)
                {
                    reconValue[index] +=
                        (reconValue[index] >= 0) ? threshold : -threshold;
                }
            }

            threshold /= 2.0;
        }

        writer.flush();
    }

    double getThreshold() const
    {
        return threshold;
    }
};

/*
=============================================================
                    EZW DECODER
=============================================================
*/

class EZWDecoder
{
private:

    int width;
    int height;

    double threshold;

    WaveletTree tree;

public:

    EZWDecoder(int w,
               int h,
               int levels,
               double t)
    {
        width = w;
        height = h;
        threshold = t;

        tree.build(width, height, levels);
    }

    vector<double> decode(
        BitReader& reader,
        int passes)
    {
        vector<double> coefficients(
            width * height, 0);

        vector<char> significant(
            width * height, 0);

        vector<char> skip(width * height, 0);

        // Accumulates across ALL passes, mirroring the
        // encoder's subordinateIndices — refinement bits are
        // sent every pass for every coefficient found
        // significant so far, not just the ones that became
        // significant in the current pass.
        vector<int> subordinateIndices;

        double currentThreshold = threshold;

        for (int pass = 0; pass < passes; pass++)
        {
            /*
            -------------------------------------------------
                    DOMINANT (SIGNIFICANCE) PASS
            -------------------------------------------------
            */

            fill(skip.begin(), skip.end(), 0);

            for (int index : tree.scanOrder)
            {
                if (significant[index] || skip[index])
                    continue;

                int symbol = reader.readBits(2);

                if (symbol == POSITIVE)
                {
                    coefficients[index] = currentThreshold;
                    significant[index] = 1;
                    subordinateIndices.push_back(index);
                }
                else if (symbol == NEGATIVE)
                {
                    coefficients[index] = -currentThreshold;
                    significant[index] = 1;
                    subordinateIndices.push_back(index);
                }
                else if (symbol == ZEROTREE)
                {
                    tree.markSubtreeSkip(index, skip);
                }
                // ZERO: nothing to do, coefficient stays 0
                // and its descendants are still coded.
            }

            /*
            -------------------------------------------------
                    SUBORDINATE (REFINEMENT) PASS
            -------------------------------------------------
            */

            for (int index : subordinateIndices)
            {
                int bit = reader.readBit();

                if (bit)
                {
                    coefficients[index] +=
                        (coefficients[index] >= 0) ? currentThreshold : -currentThreshold;
                }
            }

            currentThreshold /= 2.0;
        }

        return coefficients;
    }
};

/*
=============================================================
              SAVE COMPRESSED EZW FILE
=============================================================
*/

bool saveEZW(const string& filename,
             const EZWHeader& header,
             const vector<unsigned char>& data)
{
    ofstream file(filename, ios::binary);

    if (!file)
        return false;

    file.write("EZW2", 4);

    file.write(reinterpret_cast<const char*>(&header.paddedWidth), sizeof(int));
    file.write(reinterpret_cast<const char*>(&header.paddedHeight), sizeof(int));
    file.write(reinterpret_cast<const char*>(&header.origWidth), sizeof(int));
    file.write(reinterpret_cast<const char*>(&header.origHeight), sizeof(int));
    file.write(reinterpret_cast<const char*>(&header.levels), sizeof(int));
    file.write(reinterpret_cast<const char*>(&header.passes), sizeof(int));
    file.write(reinterpret_cast<const char*>(&header.threshold), sizeof(double));
    file.write(reinterpret_cast<const char*>(&header.bitCount), sizeof(int));

    int byteCount = static_cast<int>(data.size());

    file.write(reinterpret_cast<const char*>(&byteCount), sizeof(int));
    file.write(reinterpret_cast<const char*>(data.data()), data.size());

    return true;
}

/*
=============================================================
              LOAD COMPRESSED EZW FILE
=============================================================
*/

bool loadEZW(const string& filename,
             EZWHeader& header,
             vector<unsigned char>& data)
{
    ifstream file(filename, ios::binary);

    if (!file)
        return false;

    char magic[5] = {};

    file.read(magic, 4);

    if (string(magic) != "EZW2")
    {
        cerr << "Invalid or incompatible EZW file "
             << "(expected signature EZW2)." << endl;
        return false;
    }

    file.read(reinterpret_cast<char*>(&header.paddedWidth), sizeof(int));
    file.read(reinterpret_cast<char*>(&header.paddedHeight), sizeof(int));
    file.read(reinterpret_cast<char*>(&header.origWidth), sizeof(int));
    file.read(reinterpret_cast<char*>(&header.origHeight), sizeof(int));
    file.read(reinterpret_cast<char*>(&header.levels), sizeof(int));
    file.read(reinterpret_cast<char*>(&header.passes), sizeof(int));
    file.read(reinterpret_cast<char*>(&header.threshold), sizeof(double));
    file.read(reinterpret_cast<char*>(&header.bitCount), sizeof(int));

    int byteCount;

    file.read(reinterpret_cast<char*>(&byteCount), sizeof(int));

    data.resize(byteCount);

    file.read(reinterpret_cast<char*>(data.data()), byteCount);

    return true;
}

/*
=============================================================
                  MSE CALCULATION
=============================================================
*/

double calculateMSE(const Image& original,
                    const Image& reconstructed)
{
    if (original.pixels.size() !=
        reconstructed.pixels.size())
        return 0;

    double error = 0;

    for (size_t i = 0;
         i < original.pixels.size();
         i++)
    {
        double difference =
            original.pixels[i] -
            reconstructed.pixels[i];

        error += difference * difference;
    }

    return error /
           original.pixels.size();
}

/*
=============================================================
                  PSNR CALCULATION
=============================================================
*/

double calculatePSNR(double mse)
{
    if (mse == 0)
        return numeric_limits<double>::infinity();

    return 10.0 *
           log10(
               (255.0 * 255.0) /
               mse);
}

/*
=============================================================
             FILE SIZE CALCULATION
=============================================================
*/

long long getFileSize(const string& filename)
{
    ifstream file(filename,
                  ios::binary |
                  ios::ate);

    if (!file)
        return 0;

    return file.tellg();
}

/*
=============================================================
                  MAIN PROGRAM
=============================================================
*/

int main()
{
    cout << "=============================================\n";
    cout << "       EZW IMAGE COMPRESSION SYSTEM\n";
#ifdef USE_OPENCV
    cout << "       (JPG/PNG support enabled)\n";
#else
    cout << "       (PGM only - rebuild with -DUSE_OPENCV\n";
    cout << "        for JPG/PNG support)\n";
#endif
    cout << "=============================================\n\n";

    string inputFile;
    string compressedFile;
    string outputFile;

    cout << "Enter input image (.pgm"
#ifdef USE_OPENCV
         << ", .jpg, .jpeg or .png"
#endif
         << "): ";
    cin >> inputFile;

    cout << "Enter compressed file name: ";
    cin >> compressedFile;

    cout << "Enter reconstructed image name: ";
    cin >> outputFile;

    /*
    ---------------------------------------------------------
                       READ IMAGE
    ---------------------------------------------------------
    */

    Image original;

    if (!readImage(inputFile, original))
    {
        return 1;
    }

    cout << "\nImage loaded successfully.\n";

    cout << "Width  : "
         << original.width << endl;

    cout << "Height : "
         << original.height << endl;

    /*
    ---------------------------------------------------------
          PAD TO POWER-OF-TWO DIMENSIONS (if needed)

          Any image size is now accepted. If it isn't
          already a power of two on both axes, it is padded
          by replicating edge pixels; the padding is cropped
          back off before the reconstructed image is saved.
    ---------------------------------------------------------
    */

    int paddedWidth = nextPowerOfTwo(original.width);
    int paddedHeight = nextPowerOfTwo(original.height);

    // Guarantee a minimum size so there's at least one
    // useful decomposition level.
    paddedWidth = max(paddedWidth, 8);
    paddedHeight = max(paddedHeight, 8);

    Image padded = padImage(original, paddedWidth, paddedHeight);

    if (paddedWidth != original.width || paddedHeight != original.height)
    {
        cout << "Padded to  : " << paddedWidth << " x "
             << paddedHeight
             << " (power-of-two requirement of the wavelet transform)\n";
    }

    /*
    ---------------------------------------------------------
                 CALCULATE WAVELET LEVEL
    ---------------------------------------------------------
    */

    int levels = calculateLevels(paddedWidth, paddedHeight);

    // Keep recursion/tree depth sane for very large images.
    levels = min(levels, 8);
    levels = max(levels, 1);

    cout << "Wavelet Levels: "
         << levels << endl;

    /*
    ---------------------------------------------------------
                    COPY IMAGE DATA
    ---------------------------------------------------------
    */

    vector<double> coefficients = padded.pixels;

    /*
    ---------------------------------------------------------
                  FORWARD WAVELET
    ---------------------------------------------------------
    */

    cout << "\nPerforming Haar Wavelet Transform..."
         << endl;

    haarForward(
        coefficients,
        paddedWidth,
        paddedHeight,
        levels);

    cout << "Wavelet transform completed.\n";

    /*
    ---------------------------------------------------------
                       EZW ENCODING
    ---------------------------------------------------------
    */

    cout << "\nPerforming EZW Encoding (with zerotree pruning)..."
         << endl;

    BitWriter writer;

    EZWEncoder encoder(
        coefficients,
        paddedWidth,
        paddedHeight,
        levels);

    double initialThreshold =
        encoder.findInitialThreshold();

    int passes = 12;

    encoder.encode(writer, passes);

    const vector<unsigned char>& compressedData =
        writer.getBytes();

    EZWHeader header;

    header.paddedWidth = paddedWidth;
    header.paddedHeight = paddedHeight;
    header.origWidth = original.width;
    header.origHeight = original.height;
    header.levels = levels;
    header.passes = passes;
    header.threshold = initialThreshold;
    header.bitCount = writer.getBitCount();

    /*
    ---------------------------------------------------------
                    SAVE COMPRESSED DATA
    ---------------------------------------------------------
    */

    if (!saveEZW(
            compressedFile,
            header,
            compressedData))
    {
        cerr << "Failed to save compressed file."
             << endl;

        return 1;
    }

    cout << "EZW encoding completed.\n";

    /*
    ---------------------------------------------------------
                      DISPLAY SIZE
    ---------------------------------------------------------
    */

    long long originalSize =
        getFileSize(inputFile);

    long long compressedSize =
        getFileSize(compressedFile);

    cout << "\n=============================================\n";
    cout << "             COMPRESSION RESULTS\n";
    cout << "=============================================\n";

    cout << "Original file size   : "
         << originalSize
         << " bytes\n";

    cout << "Compressed file size : "
         << compressedSize
         << " bytes\n";

    if (compressedSize > 0)
    {
        double ratio =
            static_cast<double>(originalSize) /
            compressedSize;

        double percentage =
            (1.0 -
             static_cast<double>(compressedSize) /
             originalSize) * 100.0;

        cout << fixed
             << setprecision(2);

        cout << "Compression ratio    : "
             << ratio << ":1\n";

        cout << "Space saving         : "
             << percentage << "%\n";
    }

    /*
    ---------------------------------------------------------
                     EZW DECODING
    ---------------------------------------------------------
    */

    cout << "\nPerforming EZW Decoding..."
         << endl;

    EZWHeader loadedHeader;

    vector<unsigned char> loadedData;

    if (!loadEZW(
            compressedFile,
            loadedHeader,
            loadedData))
    {
        cerr << "Could not load EZW file."
             << endl;

        return 1;
    }

    BitReader reader(loadedData);

    EZWDecoder decoder(
        loadedHeader.paddedWidth,
        loadedHeader.paddedHeight,
        loadedHeader.levels,
        loadedHeader.threshold);

    vector<double> reconstructedCoefficients =
        decoder.decode(reader, loadedHeader.passes);

    /*
    ---------------------------------------------------------
                  INVERSE WAVELET
    ---------------------------------------------------------
    */

    cout << "Performing Inverse Haar Transform..."
         << endl;

    haarInverse(
        reconstructedCoefficients,
        loadedHeader.paddedWidth,
        loadedHeader.paddedHeight,
        loadedHeader.levels);

    /*
    ---------------------------------------------------------
               CREATE RECONSTRUCTED IMAGE
               (crop the padding back off)
    ---------------------------------------------------------
    */

    Image reconstructedPadded;

    reconstructedPadded.width = loadedHeader.paddedWidth;
    reconstructedPadded.height = loadedHeader.paddedHeight;
    reconstructedPadded.maxValue = 255;
    reconstructedPadded.pixels = reconstructedCoefficients;

    Image reconstructed = cropImage(
        reconstructedPadded,
        loadedHeader.origWidth,
        loadedHeader.origHeight);

    /*
    ---------------------------------------------------------
                    SAVE IMAGE
    ---------------------------------------------------------
    */

    if (!writeImage(
            outputFile,
            reconstructed))
    {
        cerr << "Could not save reconstructed image."
             << endl;

        return 1;
    }

    cout << "Reconstructed image saved successfully.\n";

    /*
    ---------------------------------------------------------
                  CALCULATE MSE / PSNR
                  (compared against the ORIGINAL,
                  un-padded image)
    ---------------------------------------------------------
    */

    double mse =
        calculateMSE(
            original,
            reconstructed);

    double psnr =
        calculatePSNR(mse);

    /*
    ---------------------------------------------------------
                     FINAL RESULTS
    ---------------------------------------------------------
    */

    cout << "\n=============================================\n";
    cout << "              FINAL RESULTS\n";
    cout << "=============================================\n";

    cout << fixed
         << setprecision(4);

    cout << "MSE  : "
         << mse << endl;

    cout << "PSNR : "
         << psnr << " dB\n";

    cout << "\nInput image        : "
         << inputFile << endl;

    cout << "Compressed file    : "
         << compressedFile << endl;

    cout << "Reconstructed image: "
         << outputFile << endl;

    cout << "\n=============================================\n";
    cout << "          COMPRESSION COMPLETED\n";
    cout << "=============================================\n";

    return 0;
}
