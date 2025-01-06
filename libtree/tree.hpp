#pragma once
#include <algorithm>
#include <array>
#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <cassert>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <openssl/sha.h>
#include <queue>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace boost::serialization {

    // Specialization for std::filesystem::path
    template <typename Archive>
    void serialize(Archive& ar, std::filesystem::path& p,
        unsigned int const version)
    {
        if constexpr (Archive::is_saving::value) {
            ar& p.string();
        }
        else {
            std::string buf;
            ar& buf;
            p = buf;
        }
    }

} // namespace boost::serialization

class MerkleTree {
private:
    struct FileNode {

        std::string hashString;
        std::filesystem::path filepath;
        std::array<unsigned char, 32> hash;
        FileNode* parent = nullptr;
        FileNode* firstChild = nullptr;
        FileNode* next = nullptr;
        uint64_t childNum = 0;

        bool isFolder();

        bool isDiff(FileNode const* other);

        FileNode(std::string const& time, std::filesystem::path const& path);

        FileNode(std::string s);

        FileNode(unsigned char* h);

        FileNode(FileNode* other);

        FileNode();

        // 序列化方法
        template <class Archive>
        void serialize(Archive& ar, unsigned int const version)
        {
            ar& hashString;
            ar& filepath;
            ar& childNum;
            ar& boost::serialization::make_array(hash.data(), hash.size());
        }
    };

    std::filesystem::path folder_;
    FileNode* root_ = nullptr;

    FileNode* buildTree(const std::filesystem::path& p)
    {
        assert(std::filesystem::is_directory(p) || std::filesystem::is_directory(folder_ / p));
        std::queue<FileNode*> total; // 按顺序构建

        std::vector<std::filesystem::path> paths;
        for (auto const& i : std::filesystem::directory_iterator(folder_ / p)) {
            paths.push_back(std::filesystem::relative(i, folder_));  //相对路径
        }

        // 维护一个相对稳定的顺序（使用迭代器遍历文件的顺序可能不一致）
        std::sort(paths.begin(), paths.end());

        uint64_t cnt = 0;
        for (auto const& i : paths) {
            if (std::filesystem::is_directory(folder_ / i)) {
                FileNode* now = buildTree(i);
                cnt += now->childNum;
                std::string h(reinterpret_cast<char*>(now->hash.data()), 32);
                now->filepath = i;  //相对路径
                total.push(now);
            }
            else {
                std::string t =
                    std::to_string(std::filesystem::last_write_time(folder_ / i)
                        .time_since_epoch()
                        .count()); // 最近修改时间
                FileNode* now = new FileNode(t, i);
                cnt++;
                std::string h(reinterpret_cast<char*>(now->hash.data()), 32);

                total.push(now);
            }
        }

        std::ostringstream fatherstr;

        FileNode* firstChild = nullptr;

        while (total.size() > 0) {
            FileNode* curr = total.front();
            total.pop();
            if (firstChild == nullptr)
                firstChild = curr;

            curr->next = (total.empty()) ? nullptr : total.front();
            std::string h(reinterpret_cast<char*>(curr->hash.data()), 32);
            fatherstr << h;
        }

        FileNode* current = new FileNode(fatherstr.str());
        current->filepath = p;
        current->firstChild = firstChild;
        current->childNum = cnt;

        while (firstChild != nullptr) {
            firstChild->parent = current;
            firstChild = firstChild->next;
        }

        return current;
    }

    FileNode* findFile(FileNode* folder, std::filesystem::path const& relative)
    { // 从一个父结点开始寻找当前文件夹下路径为relative的结点
        if (folder == nullptr)  return nullptr;
        FileNode* root = folder->firstChild;
        while (root != nullptr && root->filepath != relative)
            root = root->next;
        return root;
    }

    FileNode* findPre(FileNode* folder, std::filesystem::path const& relative) {
        if (folder == nullptr)  return nullptr;
        FileNode* root = folder->firstChild;

        while (root != nullptr && root->next->filepath != relative)
            root = root->next;
        return root;
    }

    void recomputeHash(FileNode* root)
    { // root是经过修改的文件结点
        if (root == nullptr)
            return;
        FileNode* iter = root->parent->firstChild;
        std::stringstream rehash;
        while (iter != nullptr) {
            std::string h(reinterpret_cast<char*>(iter->hash.data()), 32);
            rehash << h;

            iter = iter->next;
        }
        std::string f = rehash.str();
        SHA256(reinterpret_cast<unsigned char const*>(f.c_str()), f.size(),
            root->parent->hash.data());
    }

    // 新增文件节点, insert it to keep ascending property.
    void addNode(FileNode* newNode, FileNode* folder)
    {
        // Initializes newNode's parent.
        newNode->parent = folder;

        // Inserts newNode to ascending linked list.
        auto head = std::make_unique<FileNode>(); // Virtual node
        head->next = folder->firstChild;

        auto* p{ head.get() };
        while (p->next != nullptr && newNode->filepath >= p->next->filepath) {
            p = p->next;
        }
        newNode->next = p->next;
        p->next = newNode;

        folder->firstChild = head->next;

        recomputeHash(newNode);
    }

    bool deleteNode(FileNode* folder, std::filesystem::path const& file)
    { // 删除文件结点
        FileNode* f = findFile(folder, file);
        FileNode* pre = findPre(folder, file);
        if (f == nullptr) return false;

        if (pre == nullptr) {  //folder的firstChild结点
            folder->firstChild = f->next;
            delete f;
            recomputeHash(folder->firstChild);
            return true;
        }
        else {
            pre->next = f->next;
            delete f;
            recomputeHash(folder->firstChild);
            return true;
        }
    }

    void changeHash(FileNode* root, std::string const& time,
        std::filesystem::path const& path)
    { // 覆盖文件后更新哈希值
        std::string hashString = time + '|' + path.string();
        SHA256(reinterpret_cast<unsigned char const*>(hashString.c_str()),
            hashString.size(), root->hash.data());
        recomputeHash(root);
    }

    // 设A是主导文件夹，B是被同步文件夹
    // 更新准则：因为没有利用事件监听机制，只能先找到所有相对路径在A中但不在B中的文件路径，和相对路径在B中不在A中的文件路径，然后进行B的新增和删除
    // 之后A和B就能完全对应上了，让A和B一一对应地遍历，比对哈希值，如果哈希值不一致，那么覆盖更新当前指向结点所存储的相对路径的文件即可

    void syncTree(FileNode* A, FileNode* B)
    { // 哈希树的更新（不是文件的更新），用于维护当前文件夹哈希树的最新性

        if (!A || !B || !A->isFolder() || !B->isFolder()) {
            throw std::runtime_error("node error(use error)");
            return;
        }

        // 找到 A 中存在但 B 中不存在的文件或文件夹
        std::unordered_map<std::string, FileNode*> B_map;
        FileNode* currentB = B->firstChild;
        while (currentB != nullptr) {
            B_map[currentB->filepath.string()] = currentB;
            currentB = currentB->next;
        }

        FileNode* currentA = A->firstChild;
        while (currentA) {
            if (B_map.find(currentA->filepath.string()) == B_map.end()) {
                // B中不存在A的文件结点，添加到 B
                FileNode* newNode = new FileNode(currentA->hashString);
                addNode(newNode, B);
            }
            else
                B_map.erase(
                    currentA->filepath
                    .string()); // 删除所有A中的结点，这样剩下的就是B中有A中无的所有结点了
            currentA = currentA->next;
        }

        // 找到 B 中存在但 A 中不存在的文件或文件夹
        for (auto const& [filepath, node] : B_map) {
            deleteNode(B, filepath); // 删除 B 中多余的文件或文件夹
        }

        // 遍历两棵树并比对哈希值，如果不一致则更新
        currentA = A->firstChild;
        currentB = B->firstChild;
        while (currentA && currentB) {
            if (currentA->filepath == currentB->filepath) {
                if (currentA->isDiff(currentB)) {
                    // Hash 不一致，更新 B 中的文件
                    std::string time = std::to_string(
                        std::filesystem::last_write_time(currentA->filepath)
                        .time_since_epoch()
                        .count());
                    changeHash(currentB, time, currentA->filepath);
                }
                // 递归调用处理子文件夹
                if (currentA->isFolder() && currentB->isFolder()) {
                    syncTree(currentA, currentB);
                }
                currentA = currentA->next;
                currentB = currentB->next;
            }
            else {
                // 如果出现了路径不匹配的情况（不应当发生）
                throw std::runtime_error("match error(code error)");
                break;
            }
        }
    }

    // A依然为主导文件夹
    void syncFile(FileNode* A, FileNode* B, std::filesystem::path const& rootA,
        std::filesystem::path const& rootB);

    void deleteTree(FileNode* node)
    {
        if (node == nullptr)
            return;
        deleteTree(node->firstChild);
        deleteTree(node->next);
        delete node;
    }

    // 序列化哈希树至文件中
    void writeTree(std::ofstream& ofile) const
    {
        if (!ofile) {
            throw std::runtime_error("ostream error");
            return;
        }
        boost::archive::text_oarchive oa(ofile);
        oa << root_;
        ofile.close();
        std::cout << "完成保存" << std::endl;
    }

    // boost库不支持双向指针，因此只保存单向，另外一个方向的指针重新构建
    void ptrHelper(FileNode* root)
    {
        if (root == nullptr || root->firstChild == nullptr)
            return;
        FileNode* iter = root->firstChild;
        while (iter != nullptr) {
            iter->parent = root;
            ptrHelper(root); // 可能有文件夹结点
            iter = iter->next;
        }
    }

    void rebuildTree(std::ifstream& ifile)
    {
        assert(root_ == nullptr);
        if (!ifile) {
            throw std::runtime_error("istream error");
            return;
        }
        boost::archive::text_iarchive ia(ifile);
        ia >> root_;
        ptrHelper(root_);
        ifile.close();
        std::cout << "完成读取和构建" << std::endl;
    }

public:
    MerkleTree() = default;

    MerkleTree(std::string dir_path);

    bool isSame(MerkleTree* other)
    {
        return root_->hash == other->root_->hash;
    }

    void updateTree(MerkleTree* old)
    {
        syncTree(root_, old->root_);
    }

    void readTree(std::string const& filepath)
    {
        std::ifstream ifile(filepath);
        rebuildTree(ifile);
    }

    void makeTree(std::string const& filepath)
    {
        std::ofstream ofile(filepath);
        writeTree(ofile);
    }

    ~MerkleTree()
    {
        deleteTree(root_);
    }

    void sync_from(MerkleTree const& other);

    template <class Archive>
    void serialize(Archive& ar, unsigned int const version)
    {
        ar& folder_;
    }
};
