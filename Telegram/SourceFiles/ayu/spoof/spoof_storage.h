// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#pragma once

#include "base/basic_types.h"
#include "crl/crl_time.h"

#include <QtCore/QByteArray>
#include <QtCore/QString>

#include <vector>

class QImage;

namespace Ayu::Spoof {

struct Entry {
	quint64 id = 0;
	bool round = false;
	crl::time durationMs = 0;
	QString fileName;
	QString thumbFileName;
};

class Storage final {
public:
	[[nodiscard]] static Storage &Instance();

	[[nodiscard]] const std::vector<Entry> &entries() const {
		return _entries;
	}
	[[nodiscard]] int count() const {
		return static_cast<int>(_entries.size());
	}

	[[nodiscard]] QString dirPath() const;
	[[nodiscard]] QString filePath(const Entry &entry) const;
	[[nodiscard]] QString thumbPath(const Entry &entry) const;

	void load();
	[[nodiscard]] bool addFromData(
		const QByteArray &data,
		bool round,
		crl::time durationMs,
		const QImage &thumb);
	void remove(quint64 id);
	void clear();

private:
	Storage() = default;

	void save();

	std::vector<Entry> _entries;
	bool _loaded = false;

};

} // namespace Ayu::Spoof
