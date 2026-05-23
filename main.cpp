#include <algorithm>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <random>
#include <string>
#include <utility>
#include <vector>

using namespace std;

struct SortMetrics {
    long long charComparisons = 0;
};

class StringGenerator {
public:
    explicit StringGenerator(uint32_t seed = 1327597)
        : rng(seed), lengthDist(10, 200), charDist(0, alphabet().size() - 1) {}

    static const string& alphabet() {
        static const string chars =
            "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
            "abcdefghijklmnopqrstuvwxyz"
            "0123456789"
            "!@#%:;^&*()-";
        return chars;
    }

    vector<string> randomArray(size_t n) {
        vector<string> result;
        result.reserve(n);
        for (size_t i = 0; i < n; ++i) {
            result.push_back(randomString());
        }
        return result;
    }

    vector<string> reversedArray(size_t n) {
        vector<string> result = randomArray(n);
        sort(result.begin(), result.end());
        reverse(result.begin(), result.end());
        return result;
    }

    vector<string> nearlySortedArray(size_t n) {
        vector<string> result = randomArray(n);
        sort(result.begin(), result.end());
        const size_t swaps = max<size_t>(1, n / 100);
        uniform_int_distribution<size_t> indexDist(0, n - 1);
        for (size_t i = 0; i < swaps; ++i) {
            swap(result[indexDist(rng)], result[indexDist(rng)]);
        }
        return result;
    }

    vector<string> commonPrefixArray(size_t n) {
        vector<string> result;
        result.reserve(n);
        const vector<string> prefixes = {
            "CommonPrefixAlpha",
            "CommonPrefixBeta",
            "CommonPrefixGamma",
            "CommonPrefixDelta",
        };

        for (size_t i = 0; i < n; ++i) {
            string s = prefixes[i % prefixes.size()];
            const int targetLength = lengthDist(rng);
            while (static_cast<int>(s.size()) < targetLength) {
                s.push_back(randomChar());
            }
            result.push_back(s);
        }
        return result;
    }

private:
    mt19937 rng;
    uniform_int_distribution<int> lengthDist;
    uniform_int_distribution<size_t> charDist;

    char randomChar() {
        return alphabet()[charDist(rng)];
    }

    string randomString() {
        const int length = lengthDist(rng);
        string s;
        s.reserve(length);
        for (int i = 0; i < length; ++i) {
            s.push_back(randomChar());
        }
        return s;
    }
};

class StringSortTester {
public:
    explicit StringSortTester(uint32_t seed = 1327597) : generator(seed) {}

    void run(size_t maxN, size_t step, int repeats, const string& outputPath) {
        ofstream out(outputPath);
        out << "dataset,algorithm,n,avg_microseconds,avg_char_comparisons,sorted\n";

        const vector<pair<string, vector<string>>> bases = {
            {"random", generator.randomArray(maxN)},
            {"reversed", generator.reversedArray(maxN)},
            {"nearly_sorted", generator.nearlySortedArray(maxN)},
            {"common_prefix", generator.commonPrefixArray(maxN)},
        };

        const vector<pair<string, SortFunction>> algorithms = {
            {"quick_sort", [](vector<string>& a, SortMetrics& m) { quickSort(a, m); }},
            {"merge_sort", [](vector<string>& a, SortMetrics& m) { mergeSort(a, m); }},
            {"ternary_string_quick_sort", [](vector<string>& a, SortMetrics& m) { ternaryStringQuickSort(a, m); }},
            {"string_merge_sort_lcp", [](vector<string>& a, SortMetrics& m) { stringMergeSort(a, m); }},
            {"msd_radix_sort", [](vector<string>& a, SortMetrics& m) { msdRadixSort(a, m, false); }},
            {"msd_radix_sort_with_cutoff", [](vector<string>& a, SortMetrics& m) { msdRadixSort(a, m, true); }},
        };

        for (const auto& [datasetName, base] : bases) {
            for (size_t n = step; n <= maxN; n += step) {
                vector<string> sample(base.begin(), base.begin() + static_cast<long long>(n));
                for (const auto& [algorithmName, algorithm] : algorithms) {
                    RunStats stats = measure(sample, algorithm, repeats);
                    out << datasetName << ','
                        << algorithmName << ','
                        << n << ','
                        << fixed << setprecision(2) << stats.avgMicroseconds << ','
                        << fixed << setprecision(2) << stats.avgCharComparisons << ','
                        << (stats.sorted ? "true" : "false") << '\n';
                }
            }
        }
    }

private:
    using SortFunction = void (*)(vector<string>&, SortMetrics&);

    struct RunStats {
        double avgMicroseconds = 0;
        double avgCharComparisons = 0;
        bool sorted = true;
    };

    struct LcpItem {
        string value;
        size_t lcp = 0;
    };

    StringGenerator generator;

    static int charAt(const string& s, size_t depth) {
        if (depth >= s.size()) {
            return -1;
        }
        return static_cast<unsigned char>(s[depth]);
    }

    static int compareStrings(const string& a, const string& b, SortMetrics& metrics) {
        const size_t limit = min(a.size(), b.size());
        for (size_t i = 0; i < limit; ++i) {
            ++metrics.charComparisons;
            if (a[i] < b[i]) {
                return -1;
            }
            if (a[i] > b[i]) {
                return 1;
            }
        }

        ++metrics.charComparisons;
        if (a.size() < b.size()) {
            return -1;
        }
        if (a.size() > b.size()) {
            return 1;
        }
        return 0;
    }

    static pair<int, size_t> lcpCompare(const string& a, const string& b, size_t knownPrefix, SortMetrics& metrics) {
        size_t i = knownPrefix;
        const size_t limit = min(a.size(), b.size());
        while (i < limit) {
            ++metrics.charComparisons;
            if (a[i] < b[i]) {
                return {-1, i};
            }
            if (a[i] > b[i]) {
                return {1, i};
            }
            ++i;
        }

        ++metrics.charComparisons;
        if (a.size() < b.size()) {
            return {-1, i};
        }
        if (a.size() > b.size()) {
            return {1, i};
        }
        return {0, i};
    }

    static RunStats measure(const vector<string>& input, SortFunction sortFunction, int repeats) {
        RunStats stats;
        for (int i = 0; i < repeats; ++i) {
            vector<string> data = input;
            SortMetrics metrics;

            const auto started = chrono::steady_clock::now();
            sortFunction(data, metrics);
            const auto finished = chrono::steady_clock::now();

            stats.avgMicroseconds += chrono::duration<double, micro>(finished - started).count();
            stats.avgCharComparisons += static_cast<double>(metrics.charComparisons);
            stats.sorted = stats.sorted && is_sorted(data.begin(), data.end());
        }

        stats.avgMicroseconds /= repeats;
        stats.avgCharComparisons /= repeats;
        return stats;
    }

    static void quickSort(vector<string>& a, SortMetrics& metrics) {
        if (!a.empty()) {
            quickSortImpl(a, 0, static_cast<int>(a.size()) - 1, metrics);
        }
    }

    static void quickSortImpl(vector<string>& a, int left, int right, SortMetrics& metrics) {
        if (left >= right) {
            return;
        }

        const int pivotIndex = left + (right - left) / 2;
        const string pivot = a[pivotIndex];
        swap(a[left], a[pivotIndex]);

        int lt = left;
        int i = left + 1;
        int gt = right;
        while (i <= gt) {
            const int cmp = compareStrings(a[i], pivot, metrics);
            if (cmp < 0) {
                swap(a[lt++], a[i++]);
            } else if (cmp > 0) {
                swap(a[i], a[gt--]);
            } else {
                ++i;
            }
        }

        quickSortImpl(a, left, lt - 1, metrics);
        quickSortImpl(a, gt + 1, right, metrics);
    }

    static void mergeSort(vector<string>& a, SortMetrics& metrics) {
        vector<string> buffer(a.size());
        mergeSortImpl(a, buffer, 0, static_cast<int>(a.size()), metrics);
    }

    static void mergeSortImpl(vector<string>& a, vector<string>& buffer, int left, int right, SortMetrics& metrics) {
        if (right - left <= 1) {
            return;
        }

        const int middle = left + (right - left) / 2;
        mergeSortImpl(a, buffer, left, middle, metrics);
        mergeSortImpl(a, buffer, middle, right, metrics);

        int i = left;
        int j = middle;
        int k = left;
        while (i < middle && j < right) {
            if (compareStrings(a[i], a[j], metrics) <= 0) {
                buffer[k++] = std::move(a[i++]);
            } else {
                buffer[k++] = std::move(a[j++]);
            }
        }
        while (i < middle) {
            buffer[k++] = std::move(a[i++]);
        }
        while (j < right) {
            buffer[k++] = std::move(a[j++]);
        }
        for (int pos = left; pos < right; ++pos) {
            a[pos] = std::move(buffer[pos]);
        }
    }

    static void ternaryStringQuickSort(vector<string>& a, SortMetrics& metrics) {
        if (!a.empty()) {
            ternaryStringQuickSortImpl(a, 0, static_cast<int>(a.size()) - 1, 0, metrics);
        }
    }

    static void ternaryStringQuickSortImpl(vector<string>& a, int left, int right, size_t depth, SortMetrics& metrics) {
        if (left >= right) {
            return;
        }

        const int pivotIndex = left + (right - left) / 2;
        swap(a[left], a[pivotIndex]);
        const int pivot = charAt(a[left], depth);

        int lt = left;
        int gt = right;
        int i = left + 1;
        while (i <= gt) {
            const int current = charAt(a[i], depth);
            ++metrics.charComparisons;
            if (current < pivot) {
                swap(a[lt++], a[i++]);
            } else if (current > pivot) {
                swap(a[i], a[gt--]);
            } else {
                ++i;
            }
        }

        ternaryStringQuickSortImpl(a, left, lt - 1, depth, metrics);
        if (pivot >= 0) {
            ternaryStringQuickSortImpl(a, lt, gt, depth + 1, metrics);
        }
        ternaryStringQuickSortImpl(a, gt + 1, right, depth, metrics);
    }

    static void stringMergeSort(vector<string>& a, SortMetrics& metrics) {
        vector<LcpItem> items;
        items.reserve(a.size());
        for (const string& s : a) {
            items.push_back({s, 0});
        }

        items = stringMergeSortImpl(std::move(items), metrics);
        for (size_t i = 0; i < items.size(); ++i) {
            a[i] = std::move(items[i].value);
        }
    }

    static vector<LcpItem> stringMergeSortImpl(vector<LcpItem> items, SortMetrics& metrics) {
        if (items.size() <= 1) {
            if (!items.empty()) {
                items[0].lcp = 0;
            }
            return items;
        }

        const size_t middle = items.size() / 2;
        vector<LcpItem> left(items.begin(), items.begin() + static_cast<long long>(middle));
        vector<LcpItem> right(items.begin() + static_cast<long long>(middle), items.end());

        left = stringMergeSortImpl(std::move(left), metrics);
        right = stringMergeSortImpl(std::move(right), metrics);
        return stringMerge(std::move(left), std::move(right), metrics);
    }

    static vector<LcpItem> stringMerge(vector<LcpItem> left, vector<LcpItem> right, SortMetrics& metrics) {
        vector<LcpItem> result;
        result.reserve(left.size() + right.size());

        size_t i = 0;
        size_t j = 0;
        while (i < left.size() && j < right.size()) {
            if (left[i].lcp > right[j].lcp) {
                result.push_back(std::move(left[i++]));
            } else if (left[i].lcp < right[j].lcp) {
                result.push_back(std::move(right[j++]));
            } else {
                const auto [cmp, lcp] = lcpCompare(left[i].value, right[j].value, left[i].lcp, metrics);
                if (cmp <= 0) {
                    result.push_back(std::move(left[i++]));
                    if (j < right.size()) {
                        right[j].lcp = lcp;
                    }
                } else {
                    result.push_back(std::move(right[j++]));
                    if (i < left.size()) {
                        left[i].lcp = lcp;
                    }
                }
            }
        }

        while (i < left.size()) {
            result.push_back(std::move(left[i++]));
        }
        while (j < right.size()) {
            result.push_back(std::move(right[j++]));
        }
        if (!result.empty()) {
            result[0].lcp = 0;
        }
        return result;
    }

    static void msdRadixSort(vector<string>& a, SortMetrics& metrics, bool useCutoff) {
        vector<string> buffer(a.size());
        msdRadixSortImpl(a, buffer, 0, static_cast<int>(a.size()), 0, metrics, useCutoff);
    }

    static void msdRadixSortImpl(
        vector<string>& a,
        vector<string>& buffer,
        int left,
        int right,
        size_t depth,
        SortMetrics& metrics,
        bool useCutoff
    ) {
        if (right - left <= 1) {
            return;
        }

        if (useCutoff && static_cast<size_t>(right - left) < StringGenerator::alphabet().size()) {
            ternaryStringQuickSortImpl(a, left, right - 1, depth, metrics);
            return;
        }

        constexpr int alphabetSize = 256;
        vector<int> count(alphabetSize + 2, 0);

        for (int i = left; i < right; ++i) {
            ++metrics.charComparisons;
            const int c = charAt(a[i], depth);
            ++count[c + 2];
        }
        for (int r = 0; r < alphabetSize + 1; ++r) {
            count[r + 1] += count[r];
        }

        vector<int> starts = count;
        for (int i = left; i < right; ++i) {
            ++metrics.charComparisons;
            const int c = charAt(a[i], depth);
            buffer[count[c + 1]++] = std::move(a[i]);
        }
        for (int i = left; i < right; ++i) {
            a[i] = std::move(buffer[i - left]);
        }

        for (int bucket = 1; bucket <= alphabetSize; ++bucket) {
            const int bucketLeft = left + starts[bucket];
            const int bucketRight = left + starts[bucket + 1];
            if (bucketRight - bucketLeft > 1) {
                msdRadixSortImpl(a, buffer, bucketLeft, bucketRight, depth + 1, metrics, useCutoff);
            }
        }
    }
};

int main(int argc, char** argv) {
    size_t maxN = 3000;
    size_t step = 100;
    int repeats = 5;
    string outputPath = "string_sort_results.csv";

    if (argc > 1) {
        maxN = static_cast<size_t>(stoull(argv[1]));
    }
    if (argc > 2) {
        step = static_cast<size_t>(stoull(argv[2]));
    }
    if (argc > 3) {
        repeats = stoi(argv[3]);
    }
    if (argc > 4) {
        outputPath = argv[4];
    }

    StringSortTester tester;
    tester.run(maxN, step, repeats, outputPath);

    cout << "Results written to " << outputPath << '\n';
    return 0;
}
