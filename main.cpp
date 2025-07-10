#include <QtCore>
#include <exiv2/exiv2.hpp>
#include <stdio.h>
#include <iostream>

QString tryGetValue(Exiv2::ExifData &exif, const QStringList &tags)
{
    QString value;
    foreach (QString tn , tags)
    {
        Exiv2::Exifdatum& tag = exif[tn.toStdString().data()];
        value = QString::fromStdString(tag.toString());
        if(!value.isEmpty())
            break;
    }
    return value;
}

int main()
{
    bool debug = !QProcessEnvironment::systemEnvironment().value("DEBUG", "").isEmpty();

    QDir currentDir = QCoreApplication::applicationDirPath();

    QStringList datetimeTags; datetimeTags << "Exif.Photo.DateTimeOriginal"   << "Exif.Photo.DateTimeDigitized"  << "Exif.Image.DateTime"           ;
    QStringList subSecsTags; subSecsTags   << "Exif.Photo.SubSecTime"         << "Exif.Photo.SubSecTimeOriginal" << "Exif.Photo.SubSecTimeDigitized";
    QStringList timezoneTags; timezoneTags << "Exif.Photo.OffsetTime"         << "Exif.Photo.OffsetTimeOriginal" << "Exif.Photo.OffsetTimeDigitized";
    QStringList GPSTimestampTags; GPSTimestampTags << "Exif.GPSInfo.GPSTimeStamp";


    QFileInfoList list = currentDir.entryInfoList();

    for (int i = 0; i < list.size(); ++i)
    {
        QFileInfo fileInfo = list.at(i);
        QString fn = fileInfo.fileName();
        QString extention = fn.right(fn.length() - fn.lastIndexOf("."));
        extention = extention.toLower();
        while( extention == ".jpg" || extention == ".jpeg" )
        {
            auto image = Exiv2::ImageFactory::open(fn.toUtf8().data());
            image->readMetadata();
            Exiv2::ExifData &exifData = image->exifData();
            if (exifData.empty())
            {
                break;
            }
            QMap<QString, QString> tags;
            Exiv2::ExifData::const_iterator end = exifData.end();
            bool skip = false;
            QString filter_param = QProcessEnvironment::systemEnvironment().value("FILTER_NAME", "");
            QString filter_value = QProcessEnvironment::systemEnvironment().value("FILTER_VALUE", "");
            if (!filter_param.isEmpty() && !filter_value.isEmpty())
            {
                skip = true;
            }

            for (Exiv2::ExifData::const_iterator i = exifData.begin(); i != end; ++i)
            {
                QString tagName = QString::fromStdString(i->tagName());
                QString typeName = QString::fromStdString(i->typeName());
                QString Value;
                if(typeName == "Ascii")
                {
                    Value = QString::fromStdString(i->value().toString());
                }
                else if(typeName == "Short" || typeName == "Long")
                {
                    Value = QString("%1").arg(i->value().toInt64());
                }
                else if(typeName == "Rational")
                {
                    std::stringstream s;
                    s << i->value() << "\n";
                    Value = QString::fromStdString(s.str());
                }

                if(!Value.isEmpty())
                    tags[tagName] = Value;

                QString str_key = QString::fromStdString(i->key());
                // qDebug() << filter_param;
                // qDebug() << filter_value;
                // qDebug() << str_key;
                // qDebug() << Value;

                if (!filter_param.isEmpty() && !filter_value.isEmpty() && filter_param == str_key && filter_value == Value)
                {
                    skip = false;
                }
                if (debug)
                {
                    const char* tn = i->typeName();
                    std::cout << std::setw(44) << std::setfill(' ') << std::left
                              << i->key() << " "
                              << "0x" << std::setw(4) << std::setfill('0') << std::right
                              << std::hex << i->tag() << " "
                              << std::setw(9) << std::setfill(' ') << std::left
                              << (tn ? tn : "Unknown") << " "
                              << std::dec << std::setw(3)
                              << std::setfill(' ') << std::right
                              << i->count() << "  "
                              << std::dec << i->value()
                              << "\n";
                }
            }

            QString datetime = tryGetValue(exifData, datetimeTags);
            if(datetime.isEmpty())
                break;

            QDateTime dt = QDateTime::fromString(datetime, "yyyy:MM:dd hh:mm:ss");

            QString subSecs = tryGetValue(exifData, subSecsTags);
            //qDebug() << "subSecs: " << subSecs;
            if(!subSecs.isEmpty())
            {
                bool ok;
                qint32 msecs = subSecs.toInt(&ok);
                dt = dt.addMSecs(msecs%1000);
            }

            QString timeZone = tryGetValue(exifData, timezoneTags);
            QTimeZone tz;
            if(timeZone.isEmpty())
            {
                QString default_tz = QProcessEnvironment::systemEnvironment().value("SOURCE_TZ", "UTC+06:00");
                tz = QTimeZone(default_tz.toLatin1());
            }
            else
            {
                tz = QTimeZone(QByteArray("UTC")+timeZone.toLatin1());
            }

            if(tz.isValid())
            {
                dt.setTimeZone(tz);
            }
            else
            {
                qDebug() << "WARNING! Can not define source timezone for file " << fn;
            }

            QByteArray target_tz = QProcessEnvironment::systemEnvironment().value("TARGET_TZ", "UTC+06").toLatin1();
            QString name_template = QProcessEnvironment::systemEnvironment().value("NAME_TEMPLATE", "yyyyMMdd_hhmmss_zzz");

            if (debug)
            {
                QString debug3 = dt.toTimeZone(QTimeZone("Europe/Moscow")).toString("yyyyMMdd_hhmmss.zzzt");

                QString debug0 = dt.toTimeZone(QTimeZone("UTC")).toString("yyyyMMdd_hhmmss.zzzt");

                QString debug6 = dt.toTimeZone(QTimeZone("UTC+06:00")).toString("yyyyMMdd_hhmmss.zzzt");

                QString debugNsk = dt.toTimeZone(QTimeZone("Asia/Novosibirsk")).toString("yyyyMMdd_hhmmss.zzzt");
            }

            QString new_filename = dt.toTimeZone(QTimeZone(target_tz)).toString(name_template);


            if(!new_filename.isEmpty() && !skip)
            {
                qDebug() << "Filename: " << fn;
                qDebug() << "New Filename: " << new_filename;
                static QStringList files;
                int indx = 0;
                QString fullName = new_filename + extention;
                if(fullName != fn)
                {
                    while( currentDir.exists(fullName) || files.contains(fullName) )
                    {
                        fullName = new_filename + QString("_(%1)").arg(++indx) + extention;
                    }
                    if(!debug)
                    {
                        qDebug() << "Renaming!";
                        currentDir.rename(fn, fullName);
                    }
                    files.append(fullName);
                }
            }
            else
            {
                //currentDir.mkdir("__TRASH");
                //currentDir.rename(fn, "__TRASH/" + fn);
            }

            break;
        }
    }
    return 0;}
