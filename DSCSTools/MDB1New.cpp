
#include "MDB1new.h"


#include <filesystem>

namespace
{
} // namespace

namespace dscstools::mdb1new::detail
{

    inline TreeNode findFirstBitMismatch(const uint16_t first,
                                         const std::vector<TreeName>& nodeless,
                                         const std::vector<TreeName>& withNode)
    {
        if (withNode.size() == 0) return {first, 0, 0, nodeless[0]};

        for (uint16_t i = first; i < 1024; i++)
        {
            bool set   = false;
            bool unset = false;

            for (const auto& file : withNode)
            {
                if ((file.name[i >> 3] >> (i & 7)) & 1)
                    set = true;
                else
                    unset = true;

                if (set && unset) return {i, 0, 0, nodeless[0]};
            }

            auto itr = std::find_if(nodeless.begin(),
                                    nodeless.end(),
                                    [set, unset, i](const auto& file)
                                    {
                                        bool val = (file.name[i >> 3] >> (i & 7)) & 1;
                                        return val && unset || !val && set;
                                    });

            if (itr != nodeless.end()) return {i, 0, 0, *itr};
        }

        return {INVALID, INVALID, 0, ""};
    }

    inline std::string buildMDB1Path(const std::filesystem::path& path)
    {
        auto extension = path.extension().string().substr(1, 5);
        auto tmp       = path;
        auto fileName  = tmp.replace_extension("").string();

        if (extension.length() == 3) extension = extension.append(" ");
        std::replace(fileName.begin(), fileName.end(), '/', '\\');

        char name[0x81];
        strncpy(name, extension.c_str(), 4);
        strncpy(name + 4, fileName.c_str(), 0x7C);
        name[0x80] = 0; // prevent overflow

        return std::string(name);
    }

    std::vector<TreeNode> generateTree(const std::vector<std::filesystem::path> paths,
                                              std::filesystem::path source)
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

        std::vector<TreeNode> nodes  = {{INVALID, 0, 0, ""}};
        std::deque<QueueEntry> queue = {{0, INVALID, fileNames, {}, false}};

        while (!queue.empty())
        {
            QueueEntry entry = queue.front();
            queue.pop_front();
            TreeNode& parent = nodes[entry.parentNode];

            std::vector<TreeName> nodeless;
            std::vector<TreeName> withNode;

            for (auto file : entry.list)
            {
                if (std::find(entry.nodeList.begin(), entry.nodeList.end(), file) == entry.nodeList.end())
                    nodeless.push_back(file);
                else
                    withNode.push_back(file);
            }

            if (nodeless.size() == 0)
            {
                auto firstFile   = entry.list[0];
                auto itr         = std::find_if(nodes.begin(),
                                        nodes.end(),
                                        [firstFile](const TreeNode& node) { return node.name == firstFile; });
                ptrdiff_t offset = std::distance(nodes.begin(), itr);

                if (entry.isLeft)
                    parent.left = offset;
                else
                    parent.right = offset;

                continue;
            }

            TreeNode child = findFirstBitMismatch(entry.compareBit + 1, nodeless, withNode);

            if (entry.isLeft)
                parent.left = nodes.size();
            else
                parent.right = nodes.size();

            std::vector<TreeName> left;
            std::vector<TreeName> right;

            for (auto file : entry.list)
            {
                if ((file.name[child.compareBit >> 3] >> (child.compareBit & 7)) & 1)
                    right.push_back(file);
                else
                    left.push_back(file);
            }

            std::vector<TreeName> newNodeList = entry.nodeList;
            newNodeList.push_back(child.name);

            if (left.size() > 0) queue.push_front({nodes.size(), child.compareBit, std::move(left), newNodeList, true});
            if (right.size() > 0)
                queue.push_front({nodes.size(), child.compareBit, std::move(right), newNodeList, false});
            nodes.push_back(child);
        }

        return nodes;
    }

} // namespace dscstools::mdb1new::detail