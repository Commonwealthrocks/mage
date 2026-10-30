// settings.cpp
// last updated: 30/10/2026
#include "../.hpp/settings.hpp"
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QCryptographicHash>
#include <algorithm>
#include "../.hpp/__cipher.hpp"
#include "../.hpp/argon2id_hashing.hpp"
#include "../.hpp/logger.hpp"
namespace pk::cfg
{
    static QByteArray get_settings_key()
    {
        return QCryptographicHash::hash(QByteArray("__THISKEYISNOTUSEDFORENCRYPTIONRATHERMAKINGSURESETTINGSCANTGETFUCKEDWITH!!!"), QCryptographicHash::Sha256);
        // ay do not changet his hardcoded key cause the security
        // would be the same either way and this would js make me
        // have to re-do my settings; not fun.
    }
    static bool decrypt_payload(const QByteArray &payload, const QByteArray &nonce, QJsonObject &out_obj)
    {
        if (payload.isEmpty() || nonce.isEmpty())
            return false;
        try
        {
            QByteArray key = get_settings_key();
            auto cipher = pk::crypto::cipher::mk_cipher(pk::crypto::cipher::algorithm::xchacha20_poly1305);
            cipher->init(reinterpret_cast<const uint8_t *>(key.data()), key.size());
            pk::mem_::secure_vector pt = cipher->decrypt_chunk(
                reinterpret_cast<const uint8_t *>(payload.data()), payload.size(),
                nullptr, 0,
                reinterpret_cast<const uint8_t *>(nonce.data()), nonce.size());
            QByteArray raw_json(reinterpret_cast<const char *>(pt.data()), static_cast<qsizetype>(pt.size()));
            QJsonDocument plain_doc = QJsonDocument::fromJson(raw_json);
            if (!raw_json.isEmpty())
                pk::mem_::secure_wipe(raw_json.data(), static_cast<std::size_t>(raw_json.size()));
            if (!key.isEmpty())
                pk::mem_::secure_wipe(key.data(), static_cast<std::size_t>(key.size()));
            if (!plain_doc.isObject())
                return false;
            out_obj = plain_doc.object();
            return true;
        }
        catch (...)
        {
            return false;
        }
    }
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
        def_ext_behavior_v = 0;
        def_ext_open_v = false;
        def_ext_overwrite_v = 0;
        load();
    }
    QString settings::cfg_dir() const
    {
        QString app_data = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        QDir dir(app_data);
        if (dir.dirName().compare("MAGE", Qt::CaseInsensitive) == 0)
        {
            QDir parent(dir);
            if (parent.cdUp() && parent.dirName().compare("MAGE", Qt::CaseInsensitive) == 0)
            {
                dir = parent;
            }
        }
        if (dir.dirName().compare("MAGE", Qt::CaseInsensitive) != 0)
        {
            dir.setPath(QDir(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)).filePath("MAGE"));
        }
        if (!dir.exists())
        {
            dir.mkpath(".");
        }
        return dir.absolutePath();
    }
    QString settings::settings_path() const
    {
        return QDir(cfg_dir()).filePath("settings.json");
    }

    QString settings::magic_path() const
    {
        return QDir(cfg_dir()).filePath("magic.json");
    }
    void settings::parse_settings_json(const QJsonObject &obj)
    {
        if (obj.contains("def_output_path"))
        {
            def_output_path_v = obj["def_output_path"].toString();
        }
        if (obj.contains("def_ext"))
        {
            QString ext = obj["def_ext"].toString();
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
            def_cipher_v = std::clamp(obj["def_cipher"].toInt(), static_cast<int>(pk::crypto::cipher::algorithm::aes_256_gcm), static_cast<int>(pk::crypto::cipher::algorithm::aes_256_siv));
        if (obj.contains("def_time_cost"))
            def_time_cost_ = std::clamp(obj["def_time_cost"].toInt(), 1, 100);
        if (obj.contains("def_mem_cost"))
            def_mem_kbs = std::clamp(obj["def_mem_cost"].toInt(), 1024, 1048576);
        if (obj.contains("def_cores"))
            def_cores_v = std::clamp(obj["def_cores"].toInt(), 1, 64);
        if (obj.contains("def_cmp_algorithm"))
            def_cmp_algorithm_v = std::clamp(obj["def_cmp_algorithm"].toInt(), 0, 1);
        if (obj.contains("def_cmp_preset"))
            def_cmp_preset_v = std::clamp(obj["def_cmp_preset"].toInt(), 0, 3);
        if (obj.contains("def_cmp_lvl"))
            def_cmp_raw = std::clamp(obj["def_cmp_lvl"].toInt(), 0, 22);
        if (obj.contains("ss_raw_cmp"))
            __use_raw_cmp = obj["ss_raw_cmp"].toBool();
        if (obj.contains("def_ext_behavior"))
            def_ext_behavior_v = std::clamp(obj["def_ext_behavior"].toInt(), 0, 2);
        if (obj.contains("def_ext_open"))
            def_ext_open_v = obj["def_ext_open"].toBool();
        if (obj.contains("def_ext_overwrite"))
            def_ext_overwrite_v = std::clamp(obj["def_ext_overwrite"].toInt(), 0, 2);
    }
    void settings::load()
    {
        QString base_dir = cfg_dir();
        QDir dir(base_dir);
        QString set_file = settings_path();
        QString mag_file = magic_path();
        // considering the old setting structure was actual ass
        // we're doing a simple IF check
        // IF old_str = true
        //     mk_new_str
        // ELSE
        // keep current
        // --
        // --?
        if (QFile::exists(set_file) && QFile::exists(mag_file))
        {
            QFile sf(set_file);
            QFile mf(mag_file);
            if (sf.open(QIODevice::ReadOnly) && mf.open(QIODevice::ReadOnly))
            {
                QJsonDocument s_doc = QJsonDocument::fromJson(sf.readAll());
                QJsonDocument m_doc = QJsonDocument::fromJson(mf.readAll());
                if (s_doc.isObject() && m_doc.isObject())
                {
                    QJsonObject s_obj = s_doc.object();
                    QJsonObject m_obj = m_doc.object();
                    QString nonce_b64 = m_obj.contains("nonce") ? m_obj["nonce"].toString() : m_obj["n"].toString();
                    QString payload_b64 = s_obj.contains("payload") ? s_obj["payload"].toString() : s_obj["p"].toString();
                    QByteArray nonce = QByteArray::fromBase64(nonce_b64.toUtf8());
                    QByteArray payload = QByteArray::fromBase64(payload_b64.toUtf8());
                    QJsonObject plain_obj;
                    if (decrypt_payload(payload, nonce, plain_obj))
                    {
                        parse_settings_json(plain_obj);
                        pk::core::logger::log("settings decrypted and loaded from settings.json and magic.json.");
                        QString legacy_setting_cfg = dir.filePath("MAGE/mage.json");
                        if (QFile::exists(legacy_setting_cfg))
                        {
                            QFile::remove(legacy_setting_cfg);
                            dir.rmdir("MAGE");
                        }
                        QString legacy_outer = dir.filePath("mage.json");
                        if (QFile::exists(legacy_outer))
                        {
                            QFile::remove(legacy_outer);
                        }
                        return;
                    }
                    else
                    {
                        pk::core::logger::log("MAC verification failed for settings; someone tweaked them, using defaults.");
                        return;
                    }
                }
            }
        }
        QString legacy_setting_cfg = dir.filePath("MAGE/mage.json");
        if (QFile::exists(legacy_setting_cfg))
        {
            QFile nf(legacy_setting_cfg);
            if (nf.open(QIODevice::ReadOnly))
            {
                QJsonDocument n_doc = QJsonDocument::fromJson(nf.readAll());
                if (n_doc.isObject())
                {
                    QJsonObject n_obj = n_doc.object();
                    if ((n_obj.contains("n") || n_obj.contains("nonce")) &&
                        (n_obj.contains("p") || n_obj.contains("payload")))
                    {
                        QString nonce_b64 = n_obj.contains("nonce") ? n_obj["nonce"].toString() : n_obj["n"].toString();
                        QString payload_b64 = n_obj.contains("payload") ? n_obj["payload"].toString() : n_obj["p"].toString();
                        QByteArray nonce = QByteArray::fromBase64(nonce_b64.toUtf8());
                        QByteArray payload = QByteArray::fromBase64(payload_b64.toUtf8());
                        QJsonObject plain_obj;
                        if (decrypt_payload(payload, nonce, plain_obj))
                        {
                            parse_settings_json(plain_obj);
                            pk::core::logger::log("migrated settings from legacy nested MAGE/mage.json.");
                            nf.close();
                            save();
                            return;
                        }
                    }
                }
                nf.close();
            }
        }
        QString legacy_outer = dir.filePath("mage.json");
        if (QFile::exists(legacy_outer))
        {
            QFile of(legacy_outer);
            if (of.open(QIODevice::ReadOnly))
            {
                QJsonDocument o_doc = QJsonDocument::fromJson(of.readAll());
                if (o_doc.isObject())
                {
                    QJsonObject o_obj = o_doc.object();
                    if ((o_obj.contains("n") || o_obj.contains("nonce")) &&
                        (o_obj.contains("p") || o_obj.contains("payload")))
                    {
                        QString nonce_b64 = o_obj.contains("nonce") ? o_obj["nonce"].toString() : o_obj["n"].toString();
                        QString payload_b64 = o_obj.contains("payload") ? o_obj["payload"].toString() : o_obj["p"].toString();
                        QByteArray nonce = QByteArray::fromBase64(nonce_b64.toUtf8());
                        QByteArray payload = QByteArray::fromBase64(payload_b64.toUtf8());
                        QJsonObject plain_obj;
                        if (decrypt_payload(payload, nonce, plain_obj))
                        {
                            parse_settings_json(plain_obj);
                            pk::core::logger::log("migrated settings from legacy encrypted mage.json.");
                            of.close();
                            save();
                            return;
                        }
                    }
                    else if (o_obj.contains("def_chunk_size") || o_obj.contains("def_cipher") || o_obj.contains("sfx_volume"))
                    {
                        parse_settings_json(o_obj);
                        pk::core::logger::log("migrated settings from legacy unencrypted mage.json.");
                        of.close();
                        save();
                        return;
                    }
                }
                of.close();
            }
        }
        pk::core::logger::log("no existing settings found, creating default settings.json and magic.json.");
        save();
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
        obj["def_ext_behavior"] = def_ext_behavior_v;
        obj["def_ext_open"] = def_ext_open_v;
        obj["def_ext_overwrite"] = def_ext_overwrite_v;
        QJsonDocument doc(obj);
        QByteArray plain_json = doc.toJson(QJsonDocument::Compact);
        auto nonce_res = pk::crypto::kdf::mk_salt(24);
        if (!pk::crypto::kdf::ok(nonce_res.second))
            return;
        QByteArray key = get_settings_key();
        auto cipher = pk::crypto::cipher::mk_cipher(pk::crypto::cipher::algorithm::xchacha20_poly1305);
        cipher->init(reinterpret_cast<const uint8_t *>(key.data()), key.size());
        std::vector<uint8_t> ct = cipher->encrypt_chunk(
            reinterpret_cast<const uint8_t *>(plain_json.data()), plain_json.size(),
            nullptr, 0,
            nonce_res.first.data(), nonce_res.first.size());
        if (!plain_json.isEmpty())
            pk::mem_::secure_wipe(plain_json.data(), static_cast<std::size_t>(plain_json.size()));
        if (!key.isEmpty())
            pk::mem_::secure_wipe(key.data(), static_cast<std::size_t>(key.size()));
        QString nonce_b64 = QString(QByteArray(reinterpret_cast<const char *>(nonce_res.first.data()), nonce_res.first.size()).toBase64());
        QString payload_b64 = QString(QByteArray(reinterpret_cast<const char *>(ct.data()), ct.size()).toBase64());
        QJsonObject magic_obj;
        magic_obj["version"] = 1;
        magic_obj["nonce"] = nonce_b64;
        QJsonDocument magic_doc(magic_obj);
        QFile m_file(magic_path());
        if (m_file.open(QIODevice::WriteOnly))
        {
            m_file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
            m_file.write(magic_doc.toJson(QJsonDocument::Indented));
        }
        QJsonObject settings_obj;
        settings_obj["version"] = 1;
        settings_obj["payload"] = payload_b64;
        QJsonDocument settings_doc(settings_obj);

        QFile s_file(settings_path());
        if (s_file.open(QIODevice::WriteOnly))
        {
            s_file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
            s_file.write(settings_doc.toJson(QJsonDocument::Indented));
            pk::core::logger::log("settings encrypted and saved to settings.json and magic.json.");
        }
        else
        {
            pk::core::logger::log("failed to open settings.json for writing.");
        }
        QDir dir(cfg_dir());
        QString legacy_setting_cfg = dir.filePath("MAGE/mage.json");
        if (QFile::exists(legacy_setting_cfg))
        {
            QFile::remove(legacy_setting_cfg);
        }
        dir.rmdir("MAGE");
        QString legacy_outer = dir.filePath("mage.json");
        if (QFile::exists(legacy_outer))
        {
            QFile::remove(legacy_outer);
        }
    }
}

// end