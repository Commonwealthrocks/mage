// gui_worker.hpp
// last updated: 06/07/2026
#pragma once
#include <QThread>
#include <QString>
#include <vector>
#include <string>
#include "../.hpp/fileformat.hpp"
namespace pk::ui::worker
{
    class crypto_worker : public QThread
    {
        Q_OBJECT
    public:
        enum class mode
        {
            pack,
            unpack
        };
        explicit crypto_worker(mode m, QObject *parent = nullptr);
        ~crypto_worker() override = default;
        mode what_mode() const { return __mode; }
        void ss_def_pk_params(
            const std::vector<std::string> &roots,
            const QString &out_path,
            const QString &password,
            const QString &keyfile_path,
            pk::crypto::cipher::algorithm algo,
            const pk::crypto::kdf::kdf_cfg &kdf_cfg,
            bool include_hidden,
            bool cp_metadata,
            uint8_t compression_algo,
            int8_t compression_level,
            int chunk_size_mb);
        void ss_def_unpk_params(
            const QString &in_path,
            const QString &out_dir,
            const QString &password,
            const QString &keyfile_path);
    signals:
        void success();
        void error(const QString &message);
        void progress(int percentage);
        void pr_details(uint64_t processed, uint64_t total);
        void current_ac0(const QString &action);
        void current_file(const QString &file);

    protected:
        void run() override;

    private:
        mode __mode;
        std::vector<std::string> roots_f_rooted;
        std::string output_path;
        std::string password;
        std::string keyfile_path;
        pk::crypto::cipher::algorithm algo;
        pk::crypto::kdf::kdf_cfg __kdf_cfg;
        bool _include_hidden;
        bool __cp_metadata;
        int cmp_algo;
        int cmp_lvl;
        int cs_mbs;
        std::string in_path;
        std::string output_dir;
    };
}

// end