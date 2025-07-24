#define BOOST_ASIO_HAS_IO_URING 1
#include "BinaryResourceManager.hpp"
#include <memory>
#include <boost/system.hpp>
#include <boost/asio.hpp>
#include <boost/asio/stream_file.hpp>
#include <TOSLib.hpp>


namespace LV::Server {



BinaryResourceManager::BinaryResourceManager(asio::io_context &ioc)
    : AsyncObject(ioc)
{
}

BinaryResourceManager::~BinaryResourceManager() {

}

void BinaryResourceManager::recheckResources(std::vector<fs::path> assets /* Пути до активных папок assets */) {

}

void BinaryResourceManager::needResourceResponse(const ResourceRequest& resources) {
    auto lock = Local.lock();
    for(int iter = 0; iter < (int) EnumBinResource::MAX_ENUM; iter++) {
        lock->BinToHash[iter].insert(lock->BinToHash[iter].end(), resources.BinToHash[iter].begin(), resources.BinToHash[iter].end());
    }

    lock->Hashes.insert(lock->Hashes.end(), resources.Hashes.begin(), resources.Hashes.end());
}

void BinaryResourceManager::update(float dtime) {
    // if(UpdatedResources.no_lock_readable().empty())
    //     return;

    // auto lock = UpdatedResources.lock_write();
    // for(ResourceId_t resId : *lock) {
    //     auto iterPI = PreparedInformation.find(resId);
    //     if(iterPI != PreparedInformation.end())
    //         continue;

    //     auto iterRI = ResourcesInfo.find(resId);
    //     if(iterRI != ResourcesInfo.end()) {
    //         PreparedInformation[resId] = iterRI->second->Loaded;
    //     }
    // }
}

BinaryResourceManager::UriParse BinaryResourceManager::parseUri(const std::string &uri) {
    size_t pos = uri.find("://");

    if(pos == std::string::npos)
        return {"assets", uri};
    else
        return {uri.substr(0, pos), uri.substr(pos+3)};
}

// coro<> BinaryResourceManager::checkResource_Assets(ResourceId_t id, fs::path path, std::shared_ptr<Resource> res) {
//     try {
//         asio::stream_file fd(IOC, path, asio::stream_file::flags::read_only);

//         if(fd.size() > 1024*1024*16)
//             MAKE_ERROR("Превышен лимит размера файла: " << fd.size() << " > " << 1024*1024*16);

//         std::shared_ptr<ResourceFile> file = std::make_shared<ResourceFile>();
//         file->Data.resize(fd.size());
//         co_await asio::async_read(fd, asio::mutable_buffer(file->Data.data(), file->Data.size()));
//         file->calcHash();
//         res->LastError.clear();
//     } catch(const std::exception &exc) {
//         res->LastError = exc.what();
//         res->IsLoading = false;

//         if(const boost::system::system_error *errc = dynamic_cast<const boost::system::system_error*>(&exc); errc && errc->code() == asio::error::operation_aborted)
//             co_return;
//     }

//     res->IsLoading = false;
//     UpdatedResources.lock_write()->push_back(id);
// }

}