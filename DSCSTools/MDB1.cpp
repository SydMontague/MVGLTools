
#include "MDB1.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <deque>
#include <filesystem>
#include <iterator>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{
} // namespace

namespace
{
    using namespace dscstools::mdb1;

    constexpr auto isBitSet(const std::string_view name, size_t pos) -> bool
    {
        const uint64_t byte = pos >> 3;
        const uint64_t bit  = pos & 7;
        return ((name[byte] >> bit) & 1) != 0; // NOLINT(hicpp-signed-bitwise)
    }

    inline auto findFirstBitMismatch(const uint16_t first,
                                     const std::vector<TreeName>& nodeless,
                                     const std::vector<TreeName>& withNode) -> TreeNode
    {
        if (withNode.empty()) return {.compareBit = first, .left = 0, .right = 0, .name = nodeless[0]};

        for (uint16_t i = first; i < 1024; i++)
        {
            bool set   = false;
            bool unset = false;

            for (const auto& file : withNode)
            {
                if (isBitSet(file.name, i))
                    set = true;
                else
                    unset = true;

                if (set && unset) return {.compareBit = i, .left = 0, .right = 0, .name = nodeless[0]};
            }

            auto itr = std::ranges::find_if(nodeless,
                                            [set, unset, i](const auto& file)
                                            {
                                                auto val = isBitSet(file.name, i);
                                                return val && unset || !val && set;
                                            });

            if (itr != nodeless.end()) return {.compareBit = i, .left = 0, .right = 0, .name = *itr};
        }

        return {.compareBit = INVALID, .left = INVALID, .right = 0, .name{}};
    }

    inline auto buildMDB1Path(const std::filesystem::path& path) -> std::string
    {
        auto extension = path.extension().string().substr(1, 5);
        auto tmp       = path;
        auto fileName  = tmp.replace_extension("").string();

        if (extension.length() == 3) extension = extension.append(" ");
        std::ranges::replace(fileName, '/', '\\');

        std::array<char, 0x81> name{};
        strncpy(name.data(), extension.c_str(), 4);
        strncpy(name.data() + 4, fileName.c_str(), 0x7C);
        name[0x80] = '0'; // prevent overflow

        return name.data();
    }

} // namespace

namespace dscstools::mdb1::detail
{
    // NOLINTNEXTLINE(readability-function-cognitive-complexity)
    auto generateTree(const std::vector<std::filesystem::path>& paths, const std::filesystem::path& source)
        -> std::vector<TreeNode>
    {
        std::vector<TreeName> fileNames;
        std::ranges::transform(paths,
                               std::back_inserter(fileNames),
                               [&](const auto& path)
                               {
                                   auto relPath = std::filesystem::relative(path, source);
                                   return TreeName{buildMDB1Path(relPath), path};
                               });

        struct QueueEntry
        {
            uint64_t parentNode;
            uint64_t compareBit;
            std::vector<TreeName> list;
            std::vector<TreeName> nodeList;
            bool isLeft;
        };

        std::vector<TreeNode> nodes  = {{.compareBit = INVALID, .left = 0, .right = 0, .name = {}}};
        std::deque<QueueEntry> queue = {
            {.parentNode = 0, .compareBit = INVALID, .list = fileNames, .nodeList = {}, .isLeft = false}};

        while (!queue.empty())
        {
            QueueEntry entry = queue.front();
            queue.pop_front();
            TreeNode& parent = nodes[entry.parentNode];

            std::vector<TreeName> nodeless;
            std::vector<TreeName> withNode;

            for (const auto& file : entry.list)
            {
                if (std::ranges::find(entry.nodeList, file) == entry.nodeList.end())
                    nodeless.push_back(file);
                else
                    withNode.push_back(file);
            }

            if (nodeless.empty())
            {
                auto firstFile = entry.list[0];
                auto itr =
                    std::ranges::find_if(nodes, [firstFile](const TreeNode& node) { return node.name == firstFile; });
                auto offset = std::distance(nodes.begin(), itr);

                if (entry.isLeft)
                    parent.left = offset;
                else
                    parent.right = offset;

                continue;
            }

            auto child = findFirstBitMismatch(entry.compareBit + 1, nodeless, withNode);

            if (entry.isLeft)
                parent.left = nodes.size();
            else
                parent.right = nodes.size();

            std::vector<TreeName> left;
            std::vector<TreeName> right;

            for (const auto& file : entry.list)
            {
                if (isBitSet(file.name, child.compareBit))
                    right.push_back(file);
                else
                    left.push_back(file);
            }

            std::vector<TreeName> newNodeList = entry.nodeList;
            newNodeList.push_back(child.name);

            if (!left.empty()) queue.push_front({nodes.size(), child.compareBit, std::move(left), newNodeList, true});
            if (!right.empty())
                queue.push_front({nodes.size(), child.compareBit, std::move(right), newNodeList, false});
            nodes.push_back(child);
        }

        return nodes;
    }
} // namespace dscstools::mdb1::detail