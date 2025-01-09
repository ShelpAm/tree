#include <libtree/tree.hpp>

bool MerkleTree::FileNode::isFolder()
{
    namespace fs = std::filesystem;
    return fs::is_directory(filepath);
}

bool MerkleTree::FileNode::isDiff(FileNode const *other)
{
    return this->hash != other->hash;
}

MerkleTree::FileNode::FileNode(std::string const &time,
                               std::filesystem::path const &path)
    : filepath(path)
{
    hashString = time + '|' + path.string();
    SHA256(reinterpret_cast<unsigned char const *>(hashString.c_str()),
           hashString.size(), hash.data());
}
MerkleTree::FileNode::FileNode(std::string s) : hashString(std::move(s))
{
    SHA256(reinterpret_cast<unsigned char const *>(hashString.c_str()),
           hashString.size(), hash.data());
}

MerkleTree::FileNode::FileNode(unsigned char *h)
{
    std::memcpy(hash.data(), h, 32);
}

MerkleTree::FileNode::FileNode(FileNode *other)
    : hashString(other->hashString), parent(other->parent),
      firstChild(other->firstChild), next(other->next),
      childNum(other->childNum), hash{other->hash}
{
}

MerkleTree::FileNode::FileNode()
{
    hashString = "";
    SHA256(reinterpret_cast<unsigned char const *>(hashString.c_str()),
           hashString.size(), hash.data());
}

void MerkleTree::sync_from(MerkleTree const &other)
{
    syncFile(other.root_, root_, other.root_->filepath, root_->filepath);
}

MerkleTree::MerkleTree(std::string dir_path)
{
    namespace fs = std::filesystem;

    if (!fs::is_directory(dir_path)) {
        throw std::runtime_error{
            std::format("path {} isn't a directory", dir_path)};
    }
    base_dir_ = std::move(dir_path);
    root_ = buildTree(base_dir_); // 递归建树
}

void MerkleTree::syncFile(FileNode *A, FileNode *B,
                          std::filesystem::path const &rootA,
                          std::filesystem::path const &rootB)
{
    namespace fs = std::filesystem;

    // 要传递根目录的绝对路径，不然无法定位文件
    // Should be fixed here: logic error
    if (!A || !B || !A->isFolder() || !B->isFolder()) {
        throw std::runtime_error("node error(use error)");
    }

    // B有A没有
    FileNode *currentB = B->firstChild;
    while (currentB) {
        auto b_next{currentB->next};
        FileNode *correspondingA = findFile(A, currentB->filepath);
        if (!correspondingA) {
            // A 中不存在，删除B中结点对应的文件或文件夹
            fs::path targetPath = rootB / currentB->filepath;
            if (currentB->isFolder()) {
                fs::remove_all(targetPath); // 删除文件夹
            }
            else {
                fs::remove(targetPath); // 删除文件
            }
            deleteNode(B, currentB->filepath); // 删除哈希树中对应节点
        }
        currentB = b_next;
    }

    // A有B没有
    FileNode *currentA = A->firstChild;
    while (currentA) {
        FileNode *correspondingB = findFile(B, currentA->filepath);
        if (!correspondingB) {
            // B 中不存在，拷贝 A 的文件或文件夹到 B
            auto sourcePath = rootA / currentA->filepath;
            auto targetPath = rootB / currentA->filepath;
            errorln("Didn't find corresponding file in B, syncing to target "
                    "\"{}\"...",
                    targetPath.string());

            if (currentA->isFolder()) {
                fs::copy(sourcePath, targetPath,
                         fs::copy_options::recursive |
                             fs::copy_options::overwrite_existing);
            }
            else {
                fs::copy(sourcePath, targetPath,
                         fs::copy_options::overwrite_existing); // 复制文件
            }
            FileNode *newNode = new FileNode(currentA->hashString);
            newNode->filepath = currentA->filepath;
            addNode(newNode, B);
        }
        currentA = currentA->next;
    }

    // 同时遍历 A 和 B，检查哈希值不同的节点并更新
    currentA = A->firstChild;
    currentB = B->firstChild;
    while (currentA != nullptr) {
        assert(currentB != nullptr);
        errorln("a.path: {}, b.path: {}", currentA->filepath.string(),
                currentB->filepath.string());
        assert(currentA->filepath == currentB->filepath);
        // TODO(shelpam): didn't consider file type diff: directory or regular
        // file?
        if (currentA->isDiff(currentB)) {
            // 哈希值不同，覆盖更新 B 的文件
            auto sourcePath = rootA / currentA->filepath;
            auto targetPath = rootB / currentB->filepath;
            fs::copy(sourcePath, targetPath,
                     fs::copy_options::overwrite_existing); // 覆盖更新
            std::string time = std::to_string(
                fs::last_write_time(sourcePath).time_since_epoch().count());
            changeHash(currentB, time,
                       currentB->filepath); // 更新 B 的哈希值
        }
        // 递归处理子目录
        assert(currentA->isFolder() == currentB->isFolder());
        if (currentA->isFolder()) {
            syncFile(currentA, currentB, rootA, rootB);
        }
        currentA = currentA->next;
        currentB = currentB->next;
    }
}