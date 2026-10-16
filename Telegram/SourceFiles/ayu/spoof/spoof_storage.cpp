// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#include "ayu/spoof/spoof_storage.h"

#include "base/random.h"
#include "core/application.h"
#include "core/core_settings.h"

#include <algorithm>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtGui/QImage>

namespace Ayu::Spoof {
namespace {

const auto kIndexName = u"index.json"_q;

} // namespace

QString Storage::dirPath() const {
	return cWorkingDir() + u"tdata/ayu/spoof_panel/"_q;
}

QString Storage::filePath(const Entry &entry) const {
	return dirPath() + entry.fileName;
}

QString Storage::thumbPath(const Entry &entry) const {
	return entry.thumbFileName.isEmpty()
		? QString()
		: dirPath() + entry.thumbFileName;
}

void Storage::load() {
	if (_loaded) {
		return;
	}
	_loaded = true;

	_entries.clear();
	auto file = QFile(dirPath() + kIndexName);
	if (!file.open(QIODevice::ReadOnly)) {
		return;
	}
	const auto doc = QJsonDocument::fromJson(file.readAll());
	if (!doc.isArray()) {
		return;
	}
	for (const auto value : doc.array()) {
		const auto object = value.toObject();
		auto entry = Entry();
		entry.id = static_cast<quint64>(object[u"id"_q].toDouble());
		entry.round = object[u"round"_q].toBool();
		entry.durationMs = static_cast<crl::time>(object[u"duration"_q].toDouble());
		entry.fileName = object[u"file"_q].toString();
		entry.thumbFileName = object[u"thumb"_q].toString();
		if (!entry.id || entry.fileName.isEmpty()) {
			continue;
		}
		if (!QFile::exists(filePath(entry))) {
			continue;
		}
		_entries.push_back(std::move(entry));
	}
}

void Storage::save() {
	QDir().mkpath(dirPath());
	auto array = QJsonArray();
	for (const auto &entry : _entries) {
		array.push_back(QJsonObject{
			{ u"id"_q, static_cast<double>(entry.id) },
			{ u"round"_q, entry.round },
			{ u"duration"_q, static_cast<double>(entry.durationMs) },
			{ u"file"_q, entry.fileName },
			{ u"thumb"_q, entry.thumbFileName },
		});
	}
	auto file = QFile(dirPath() + kIndexName);
	if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
		return;
	}
	file.write(QJsonDocument(array).toJson(QJsonDocument::Compact));
}

bool Storage::addFromData(
		const QByteArray &data,
		bool round,
		crl::time durationMs,
		const QImage &thumb) {
	load();
	if (data.isEmpty()) {
		return false;
	}

	QDir().mkpath(dirPath());
	const auto id = base::RandomValue<quint64>() | 1;
	auto entry = Entry();
	entry.id = id;
	entry.round = round;
	entry.durationMs = std::max(durationMs, crl::time(0));
	entry.fileName = QString::number(id)
		+ (round ? u".mp4"_q : u".ogg"_q);
	entry.thumbFileName = thumb.isNull()
		? QString()
		: QString::number(id) + u".png"_q;

	auto file = QFile(filePath(entry));
	if (!file.open(QIODevice::WriteOnly)
		|| file.write(data) != data.size()) {
		return false;
	}
	file.close();
	if (!entry.thumbFileName.isEmpty() && !thumb.save(thumbPath(entry))) {
		entry.thumbFileName.clear();
	}
	_entries.push_back(std::move(entry));
	save();
	return true;
}

void Storage::remove(quint64 id) {
	load();
	const auto it = std::find_if(_entries.begin(), _entries.end(), [&](const Entry &entry) {
		return entry.id == id;
	});
	if (it == _entries.end()) {
		return;
	}
	QFile::remove(filePath(*it));
	if (!it->thumbFileName.isEmpty()) {
		QFile::remove(thumbPath(*it));
	}
	_entries.erase(it);
	save();
}

void Storage::clear() {
	load();
	for (const auto &entry : _entries) {
		QFile::remove(filePath(entry));
		if (!entry.thumbFileName.isEmpty()) {
			QFile::remove(thumbPath(entry));
		}
	}
	_entries.clear();
	save();
}

Storage &Storage::Instance() {
	static Storage instance;
	return instance;
}

} // namespace Ayu::Spoof
