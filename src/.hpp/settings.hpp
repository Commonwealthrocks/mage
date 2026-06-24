// settings.hpp
// last updated: 17/06/2026
#pragma once
#include <QString>
#include <QJsonObject>
namespace pk::cfg
{
    class settings
    {
    public:
        static settings &instance();
        void load();
        void save() const;
        QString def_output_path() const { return def_output_path_v; }
        void ss_def_output_path(const QString &path) { def_output_path_v = path; }
        QString def_ext() const { return def_ext__; }
        void ss_def_ext(const QString &ext) { def_ext__ = ext; }
        bool mute_sfx() const { return mute_sfx_v; }
        void ss_def_mute_sfx(bool mute) { mute_sfx_v = mute; }
        int sfx_volume() const { return sfx_vol; }
        void ss_def_sfx_vol(int vol) { sfx_vol = vol; }
        int def_chunk_size() const { return def_cs_mbs; }
        void ss_def_chunk_size(int mb) { def_cs_mbs = mb; }
        bool include_hidden() const { return _include_hidden; }
        void ss_def_include_hidden(bool include) { _include_hidden = include; }
        bool cp_metadata() const { return __cp_metadata; }
        void set_cp_metadata(bool copy) { __cp_metadata = copy; }
        int def_cipher() const { return def_cipher_v; }
        void ss_def_cipher(int cipher) { def_cipher_v = cipher; }
        int def_time_cost() const { return def_time_cost_; }
        void ss_def_tc(int cost) { def_time_cost_ = cost; }
        int def_mem_cost() const { return def_mem_kbs; }
        void ss_def_mem_kbs(int kb) { def_mem_kbs = kb; }
        int def_cores() const { return def_cores_v; }
        void ss_def_cores(int par) { def_cores_v = par; }
        int def_cmp_algorithm() const { return def_cmp_algorithm_v; }
        void ss_def_cmp(int algo) { def_cmp_algorithm_v = algo; }
        int def_cmp_preset() const { return def_cmp_preset_v; }
        void ss_def_cmp_preset(int preset) { def_cmp_preset_v = preset; }
        int def_cmp_lvl() const { return def_cmp_raw; }
        void ss_def_cmp_raw(int raw) { def_cmp_raw = raw; }
        bool ss_raw_cmp() const { return __use_raw_cmp; }
        void ss_def_use_raw_cmp(bool use_raw) { __use_raw_cmp = use_raw; }

    private:
        settings();
        ~settings() = default;
        QString settings_path() const;
        QString def_output_path_v;
        QString def_ext__;
        bool mute_sfx_v;
        int sfx_vol;
        bool _include_hidden;
        bool __cp_metadata;
        int def_cs_mbs;
        int def_cipher_v; // 0 = aes256gcm | 1 = xchacha20poly1305
        int def_time_cost_;
        int def_mem_kbs;
        int def_cores_v;
        int def_cmp_algorithm_v;
        int def_cmp_preset_v;
        int def_cmp_raw;
        bool __use_raw_cmp;
    };
}

// end