/**
  * Touhou Community Reliant Automatic Patcher
  * Loader
  *
  * ----
  *
  * Pure Qt rewrite.
  */

#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonDocument>
#include <QtWidgets/QApplication>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QPushButton>
extern "C" {
	#include <thcrap.h>
}

#define HTML_LINK(file) "<tt><a href=\""#file"\">"#file"</a></tt>"

const char *EXE_HELP =
	"The <i>executable</i> can either be a game ID which is then looked "
	"up in <tt>games.js</tt>, or simply the relative or absolute path to "
	"an .exe file.<br />"
	"It can either be given as a command line parameter, or through the "
	"run configuration as a <tt>\"game\"</tt> value for a game ID or an "
	"<tt>\"exe\"</tt> value for an .exe file path. If both are given, "
	"<tt>\"exe\"</tt> takes precedence.";

int QJsonParseErrorMessage(const QJsonParseError& err, const QByteArray& data, const QString& source)
{
	if(err.error == QJsonParseError::NoError) {
		return QMessageBox::Ok;
	}
	int line = 1;
	int col = 0;
	int i = 0;

	// Skip UTF-8 byte order mark, if there
	const unsigned char utf8_bom[] = {0xef, 0xbb, 0xbf, 0};
	if(data.startsWith((const char *)utf8_bom)) {
		i = sizeof(utf8_bom) - 1;
	}
	for(; i < err.offset; i++) {
		col++;
		if(data[i] == '\n') {
			col = 0;
			line++;
		}
	}
	// TODO: Show at least 2 lines of context, with a nice UTF-8 arrow
	// pointing to the exact position of the parse error.
	QMessageBox msg;
	msg.setIcon(QMessageBox::Critical);
	msg.setText(QString(
		"JSON parsing error: %1 (%2, line %3, column %4)"
		).arg(err.errorString()
		).arg(source
		).arg(line
		).arg(col
	));
	msg.addButton(QMessageBox::Ok);
	msg.addButton(QMessageBox::Retry);
	return msg.exec();
}

QJsonDocument QJsonDocumentFromFileReport(const QString& fn)
{
	QJsonParseError err;
	QByteArray data;
	QJsonDocument ret;
	do {
		QFile file(fn);
		if(!file.open(QIODevice::ReadOnly)) {
			return QJsonDocument();
		}
		data = file.readAll();
		ret = QJsonDocument::fromJson(data, &err);
	} while(QJsonParseErrorMessage(err, data, fn) == QMessageBox::Retry);
	return ret;
}

int main(int argc, char **argv)
{
	QApplication app(argc, argv);
	app.setApplicationName(PROJECT_NAME());
	app.setApplicationVersion(PROJECT_VERSION_STRING());

	if(argc < 2) {
		QMessageBox msg;
		msg.setIcon(QMessageBox::Information);
		msg.setText(QString(
			"This is the command-line loader component of the %1."
		).arg(PROJECT_NAME()));
		msg.setInformativeText(QString("<p>"
			"You are probably looking for the configuration tool to set "
			"up shortcuts for the simple usage of the patcher - that would be "
			HTML_LINK(thcrap_configure)"."
			"</p><p>"
			"If not, this is how to use the loader directly:"
			"</p><p>"
			"<tt>thcrap_loader runconfig.js <i>executable</i></tt>"
			"</p><ul>"
			"<li>The run configuration file must end in .js to be recognized as such.</li>\n"
			"<li>"
			"<li>%1</li>"
			"<li>Also, later command-line parameters take priority over earlier ones.</li>"
			"</ul>"
			).arg(EXE_HELP
		));
		msg.exec();
		return -1;
	}

	// Make sure to get the arguments in Unicode
	auto args = app.arguments();

	auto games = QJsonDocumentFromFileReport(
		app.applicationDirPath() + "/games.js"
	).object();

	// Parse command line
	bool run_cfg_seen = false; // We don't display an error if we have
	QString run_cfg_fn;

	QString cmd_exe_fn; // .exe file name from the command-line
	QString cfg_exe_fn; // .exe file name from the run configuration

	QString game_missing;

	auto game_lookup = [&game_missing, &games](const QString& game) {
		auto& ret = games[game].toString();
		if(ret.isEmpty()) {
			game_missing = game;
		}
		return ret;
	};
	for(int i = 1; i < args.size(); i++) {
		auto& arg = args[i];
		auto arg_ext = QFileInfo(args[i]).suffix();

		if(!arg_ext.compare("js", Qt::CaseInsensitive)) {
			run_cfg_seen = true;
			auto run_cfg = QJsonDocumentFromFileReport(arg);
			if(!run_cfg.isNull()) {
				run_cfg_fn = arg;

				auto new_exe_fn = run_cfg.object()["exe"].toString();
				if(new_exe_fn.isEmpty()) {
					auto& game = run_cfg.object()["game"].toString();
					if(!game.isEmpty()) {
						new_exe_fn = game_lookup(game);
					}
				}
				if(!new_exe_fn.isEmpty()) {
					cfg_exe_fn = new_exe_fn;
				}
			}
		} else if(!arg_ext.compare("exe", Qt::CaseInsensitive)) {
			cmd_exe_fn = arg;
		} else {
			// Need to set game_missing even if games is null.
			cmd_exe_fn = game_lookup(arg);
		}
	}

	if(run_cfg_fn.isEmpty()) {
		if(!run_cfg_seen) {
			QMessageBox msg;
			msg.setIcon(QMessageBox::Critical);
			msg.setText("No run configuration .js file given.");
			msg.setInformativeText(
				"If you do not have one yet, use the "
				HTML_LINK(thcrap_configure)" tool to create one."
			);
			msg.exec();
		}
		return -2;
	}

	auto& final_exe_fn = !cmd_exe_fn.isEmpty() ? cmd_exe_fn : cfg_exe_fn;
	if(final_exe_fn.isEmpty()) {
		QMessageBox msg;
		msg.setIcon(QMessageBox::Critical);
		if(!game_missing.isEmpty() || games.isEmpty()) {
			msg.setText(QString(
				"The game ID <tt>\"%1\"</tt> is missing in <tt>games.js</tt>."
				).arg(game_missing)
			);
		} else {
			msg.setText("No target executable given.");
			msg.setInformativeText(EXE_HELP);
		}
		msg.exec();
		return -3;
	}
	return thcrap_inject_into_new(
		final_exe_fn.toUtf8().data(), NULL, run_cfg_fn.toUtf8().data()
	);
}
