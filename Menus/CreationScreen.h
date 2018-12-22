#pragma once

#include "ui_CreationScreen.h"
#include <QDialog.h>

class CreationScreen : public QDialog {
	Q_OBJECT

public:
	CreationScreen(QWidget* parent = nullptr, const fs::path = "");
	Ui::CreationScreen ui;
	void save(const fs::path path) const;
};