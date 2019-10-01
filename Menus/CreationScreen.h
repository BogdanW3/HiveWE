#pragma once

#include "ui_CreationScreen.h"
#include <QDialog.h>
#include <filesystem>

namespace fs = std::filesystem;
class CreationScreen : public QDialog {
	Q_OBJECT

public:
	CreationScreen(QWidget* parent = nullptr, const fs::path = "");
	Ui::CreationScreen ui;
	void create(const fs::path path) const;
};