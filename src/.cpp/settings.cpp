// settings.cpp
// last updated: 17/06/2026
#include "../.hpp/settings.hpp"
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <algorithm>
#include "../.hpp/__cipher.hpp"
namespace pk::cfg
{
    settings &settings::instance()
    {
        static settings s_instance;
        return s_instance;
    }
    settings::settings()
    {
        def_output_path_v = ".";
        def_ext__ = ".mage";
        mute_sfx_v = false;
        sfx_vol = 50;
        _include_hidden = false;
        __cp_metadata = false;
        def_cs_mbs = 4;
        def_cipher_v = 0;
        def_time_cost_ = 3;
        def_mem_kbs = 65536;
        def_cores_v = 1;
        def_cmp_algorithm_v = 0;
        def_cmp_preset_v = 1;
        def_cmp_raw = 3;
        __use_raw_cmp = false;
        load();
    }
    QString settings::settings_path() const
    {
        QString app_data = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        QDir dir(app_data);
        if (!dir.exists())
        {
            dir.mkpath(".");
        }
        return dir.filePath("mage.json");
    }
    void settings::load()
    {
        QFile file(settings_path());
        if (!file.open(QIODevice::ReadOnly))
            return;
        QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        if (!doc.isObject())
            return;
        QJsonObject obj = doc.object();
        if (obj.contains("def_output_path"))
        {
            def_output_path_v = obj["def_output_path"].toString();
        }
        if (obj.contains("def_ext"))
        {
            QString ext = obj["def_ext"].toString();
            // validate extensions or paths, even if windows probably can handle this
            if (ext.startsWith('.') && !ext.contains('/') && !ext.contains('\\') && !ext.contains(".."))
            {
                def_ext__ = ext;
            }
            else
            {
                def_ext__ = ".mage";
            }
        }
        else
        {
            def_ext__ = ".mage";
        }
        // clamps
        if (obj.contains("mute_sfx"))
            mute_sfx_v = obj["mute_sfx"].toBool();
        if (obj.contains("sfx_volume"))
            sfx_vol = std::clamp(obj["sfx_volume"].toInt(), 0, 100);
        if (obj.contains("include_hidden"))
            _include_hidden = obj["include_hidden"].toBool();
        if (obj.contains("cp_metadata"))
            __cp_metadata = obj["cp_metadata"].toBool();
        if (obj.contains("def_chunk_size"))
            def_cs_mbs = std::clamp(obj["def_chunk_size"].toInt(), 1, 64);
        if (obj.contains("def_cipher"))
            def_cipher_v = std::clamp(obj["def_cipher"].toInt(), static_cast<int>(pk::crypto::cipher::algorithm::aes_256_gcm), static_cast<int>(pk::crypto::cipher::algorithm::xchacha20_poly1305));
        if (obj.contains("def_time_cost"))
            def_time_cost_ = std::clamp(obj["def_time_cost"].toInt(), 1, 100);
        if (obj.contains("def_mem_cost"))
            def_mem_kbs = std::clamp(obj["def_mem_cost"].toInt(), 1024, 1048576);
        if (obj.contains("def_cores"))
            def_cores_v = std::clamp(obj["def_cores"].toInt(), 1, 64);
        if (obj.contains("def_cmp_algorithm"))
            def_cmp_algorithm_v = std::clamp(obj["def_cmp_algorithm"].toInt(), 0, 2);
        if (obj.contains("def_cmp_preset"))
            def_cmp_preset_v = std::clamp(obj["def_cmp_preset"].toInt(), 0, 2);
        if (obj.contains("def_cmp_lvl"))
            def_cmp_raw = std::clamp(obj["def_cmp_lvl"].toInt(), 1, 22);
        if (obj.contains("ss_raw_cmp"))
            __use_raw_cmp = obj["ss_raw_cmp"].toBool();
    }
    void settings::save() const
    {
        QJsonObject obj;
        obj["def_output_path"] = def_output_path_v;
        obj["def_ext"] = def_ext__;
        obj["mute_sfx"] = mute_sfx_v;
        obj["sfx_volume"] = sfx_vol;
        obj["include_hidden"] = _include_hidden;
        obj["cp_metadata"] = __cp_metadata;
        obj["def_chunk_size"] = def_cs_mbs;
        obj["def_cipher"] = def_cipher_v;
        obj["def_time_cost"] = (int)def_time_cost_;
        obj["def_mem_cost"] = (int)def_mem_kbs;
        obj["def_cores"] = (int)def_cores_v;
        obj["def_cmp_algorithm"] = def_cmp_algorithm_v;
        obj["def_cmp_preset"] = def_cmp_preset_v;
        obj["def_cmp_lvl"] = def_cmp_raw;
        obj["ss_raw_cmp"] = __use_raw_cmp;
        QJsonDocument doc(obj);
        QFile file(settings_path());
        if (file.open(QIODevice::WriteOnly))
        {
            file.write(doc.toJson());
        }
    }
}

// end