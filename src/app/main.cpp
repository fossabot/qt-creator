/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of Qt Creator.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
****************************************************************************/

#include "../tools/qtcreatorcrashhandler/crashhandlersetup.h"

#include <app/app_version.h>
#include <extensionsystem/iplugin.h>
#include <extensionsystem/pluginerroroverview.h>
#include <extensionsystem/pluginmanager.h>
#include <extensionsystem/pluginspec.h>
#include <qtsingleapplication.h>
#include <utils/fileutils.h>
#include <utils/hostosinfo.h>

#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QLibraryInfo>
#include <QLoggingCategory>
#include <QSettings>
#include <QStyle>
#include <QTextStream>
#include <QThreadPool>
#include <QTimer>
#include <QTranslator>
#include <QUrl>
#include <QVariant>

#include <QNetworkProxyFactory>

#include <QApplication>
#include <QMessageBox>
#include <QStandardPaths>
#include <QTemporaryDir>
//OPENMV-DIFF//
#include <QtGlobal>
//OPENMV-DIFF//

#ifdef ENABLE_QT_BREAKPAD
#include <qtsystemexceptionhandler.h>
#endif

using namespace ExtensionSystem;

enum { OptionIndent = 4, DescriptionIndent = 34 };

//OPENMV-DIFF//
//const char appNameC[] = "Qt Creator";
//OPENMV-DIFF//
const char appNameC[] = "OpenMV IDE";
//OPENMV-DIFF//
const char corePluginNameC[] = "Core";
const char fixedOptionsC[] =
" [OPTION]... [FILE]...\n"
"Options:\n"
"    -help                         Display this help\n"
"    -version                      Display program version\n"
//OPENMV-DIFF//
//"    -client                       Attempt to connect to already running first instance\n"
//OPENMV-DIFF//
"    -settingspath <path>          Override the default path where user settings are stored\n"
//OPENMV-DIFF//
//"    -pid <pid>                    Attempt to connect to instance given by pid\n"
//"    -block                        Block until editor is closed\n"
//OPENMV-DIFF//
"    -pluginpath <path>            Add a custom search path for plugins\n";

const char HELP_OPTION1[] = "-h";
const char HELP_OPTION2[] = "-help";
const char HELP_OPTION3[] = "/h";
const char HELP_OPTION4[] = "--help";
const char VERSION_OPTION[] = "-version";
const char CLIENT_OPTION[] = "-client";
const char SETTINGS_OPTION[] = "-settingspath";
const char TEST_OPTION[] = "-test";
//OPENMV-DIFF//
//const char PID_OPTION[] = "-pid";
//const char BLOCK_OPTION[] = "-block";
//OPENMV-DIFF//
const char PLUGINPATH_OPTION[] = "-pluginpath";

typedef QList<PluginSpec *> PluginSpecSet;

// Helpers for displaying messages. Note that there is no console on Windows.

// Format as <pre> HTML
static inline QString toHtml(const QString &t)
{
    QString res = t;
    res.replace(QLatin1Char('&'), QLatin1String("&amp;"));
    res.replace(QLatin1Char('<'), QLatin1String("&lt;"));
    res.replace(QLatin1Char('>'), QLatin1String("&gt;"));
    res.insert(0, QLatin1String("<html><pre>"));
    res.append(QLatin1String("</pre></html>"));
    return res;
}

static void displayHelpText(const QString &t)
{
    if (Utils::HostOsInfo::isWindowsHost())
        QMessageBox::information(0, QLatin1String(appNameC), toHtml(t));
    else
        //OPENMV-DIFF//
        //qWarning("%s", qPrintable(t));
        //OPENMV-DIFF//
        qInfo("%s", qPrintable(t));
        //OPENMV-DIFF//
}

static void displayError(const QString &t)
{
    if (Utils::HostOsInfo::isWindowsHost())
        QMessageBox::critical(0, QLatin1String(appNameC), t);
    else
        qCritical("%s", qPrintable(t));
}

static void printVersion(const PluginSpec *coreplugin)
{
    QString version;
    QTextStream str(&version);
    //OPENMV-DIFF//
    //str << '\n' << appNameC << ' ' << coreplugin->version()<< " based on Qt " << qVersion() << "\n\n";
    //PluginManager::formatPluginVersions(str);
    //str << '\n' << coreplugin->copyright() << '\n';
    //OPENMV-DIFF//
    Q_UNUSED(coreplugin)
    str << '\n' << appNameC << ' ' << QLatin1String(Core::Constants::OMV_IDE_VERSION_LONG) << " based on Qt " << qVersion();
    //OPENMV-DIFF//
    displayHelpText(version);
}

static void printHelp(const QString &a0)
{
    QString help;
    QTextStream str(&help);
    str << "Usage: " << a0 << fixedOptionsC;
    //OPENMV-DIFF//
    //PluginManager::formatOptions(str, OptionIndent, DescriptionIndent);
    //OPENMV-DIFF//
    PluginManager::formatPluginOptions(str, OptionIndent, DescriptionIndent);
    displayHelpText(help);
}

static inline QString msgCoreLoadFailure(const QString &why)
{
    return QCoreApplication::translate("Application", "Failed to load core: %1").arg(why);
}

static inline int askMsgSendFailed()
{
    return QMessageBox::question(0, QApplication::translate("Application","Could not send message"),
                                 QCoreApplication::translate("Application", "Unable to send command line arguments to the already running instance. "
                                                             "It appears to be not responding. Do you want to start a new instance of Creator?"),
                                 QMessageBox::Yes | QMessageBox::No | QMessageBox::Retry,
                                 QMessageBox::Retry);
}

static const char *setHighDpiEnvironmentVariable()
{
    const char* envVarName = 0;
    static const char ENV_VAR_QT_DEVICE_PIXEL_RATIO[] = "QT_DEVICE_PIXEL_RATIO";
#if (QT_VERSION < QT_VERSION_CHECK(5, 6, 0))
    if (Utils::HostOsInfo().isWindowsHost()
            && !qEnvironmentVariableIsSet(ENV_VAR_QT_DEVICE_PIXEL_RATIO)) {
        envVarName = ENV_VAR_QT_DEVICE_PIXEL_RATIO;
        qputenv(envVarName, "auto");
    }
#else
    //OPENMV-DIFF//
    //if (Utils::HostOsInfo().isWindowsHost()
    //OPENMV-DIFF//
    if ((Utils::HostOsInfo().isWindowsHost() || Utils::HostOsInfo().isLinuxHost())
    //OPENMV-DIFF//
            && !qEnvironmentVariableIsSet(ENV_VAR_QT_DEVICE_PIXEL_RATIO) // legacy in 5.6, but still functional
            && !qEnvironmentVariableIsSet("QT_AUTO_SCREEN_SCALE_FACTOR")
            && !qEnvironmentVariableIsSet("QT_SCALE_FACTOR")
            && !qEnvironmentVariableIsSet("QT_SCREEN_SCALE_FACTORS")) {
        QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    }
#endif // < Qt 5.6
    return envVarName;
}

// taken from utils/fileutils.cpp. We can not use utils here since that depends app_version.h.
static bool copyRecursively(const QString &srcFilePath,
                            const QString &tgtFilePath)
{
    QFileInfo srcFileInfo(srcFilePath);
    if (srcFileInfo.isDir()) {
        QDir targetDir(tgtFilePath);
        targetDir.cdUp();
        if (!targetDir.mkdir(Utils::FileName::fromString(tgtFilePath).fileName()))
            return false;
        QDir sourceDir(srcFilePath);
        QStringList fileNames = sourceDir.entryList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System);
        foreach (const QString &fileName, fileNames) {
            const QString newSrcFilePath
                    = srcFilePath + QLatin1Char('/') + fileName;
            const QString newTgtFilePath
                    = tgtFilePath + QLatin1Char('/') + fileName;
            if (!copyRecursively(newSrcFilePath, newTgtFilePath))
                return false;
        }
    } else {
        if (!QFile::copy(srcFilePath, tgtFilePath))
            return false;
    }
    return true;
}

static inline QStringList getPluginPaths()
{
    QStringList rc;
    // Figure out root:  Up one from 'bin'
    QDir rootDir = QApplication::applicationDirPath();
    rootDir.cdUp();
    const QString rootDirPath = rootDir.canonicalPath();
    QString pluginPath;
    if (Utils::HostOsInfo::isMacHost()) {
        // 1) "PlugIns" (OS X)
        pluginPath = rootDirPath + QLatin1String("/PlugIns");
        rc.push_back(pluginPath);
    } else {
        // 2) "plugins" (Win/Linux)
        pluginPath = rootDirPath;
        pluginPath += QLatin1Char('/');
        pluginPath += QLatin1String(IDE_LIBRARY_BASENAME);
        pluginPath += QLatin1String("/qtcreator/plugins");
        rc.push_back(pluginPath);
    }
    // 3) <localappdata>/plugins/<ideversion>
    //    where <localappdata> is e.g.
    //    "%LOCALAPPDATA%\QtProject\qtcreator" on Windows Vista and later
    //    "$XDG_DATA_HOME/data/QtProject/qtcreator" or "~/.local/share/data/QtProject/qtcreator" on Linux
    //    "~/Library/Application Support/QtProject/Qt Creator" on Mac
    pluginPath = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
#if defined(Q_OS_UNIX) && !defined(Q_OS_MAC)
    pluginPath += QLatin1String("/data");
#endif
    pluginPath += QLatin1Char('/')
            + QLatin1String(Core::Constants::IDE_SETTINGSVARIANT_STR)
            + QLatin1Char('/');
    pluginPath += QLatin1String(Utils::HostOsInfo::isMacHost() ? "Qt Creator" : "qtcreator");
    pluginPath += QLatin1String("/plugins/");
    pluginPath += QLatin1String(Core::Constants::IDE_VERSION_LONG);
    rc.push_back(pluginPath);
    return rc;
}

static QSettings *createUserSettings()
{
    return new QSettings(QSettings::IniFormat, QSettings::UserScope,
                         QLatin1String(Core::Constants::IDE_SETTINGSVARIANT_STR),
                         //OPENMV-DIFF/
                         //QLatin1String("QtCreator"));
                         //OPENMV-DIFF//
                         QLatin1String("OpenMVIDE"));
                         //OPENMV-DIFF//
}

static inline QSettings *userSettings()
{
    QSettings *settings = createUserSettings();
    const QString fromVariant = QLatin1String(Core::Constants::IDE_COPY_SETTINGS_FROM_VARIANT_STR);
    if (fromVariant.isEmpty())
        return settings;

    // Copy old settings to new ones:
    QFileInfo pathFi = QFileInfo(settings->fileName());
    if (pathFi.exists()) // already copied.
        return settings;

    QDir destDir = pathFi.absolutePath();
    if (!destDir.exists())
        destDir.mkpath(pathFi.absolutePath());

    QDir srcDir = destDir;
    srcDir.cdUp();
    if (!srcDir.cd(fromVariant))
        return settings;

    if (srcDir == destDir) // Nothing to copy and no settings yet
        return settings;

    QStringList entries = srcDir.entryList();
    foreach (const QString &file, entries) {
        const QString lowerFile = file.toLower();
        if (lowerFile.startsWith(QLatin1String("profiles.xml"))
                || lowerFile.startsWith(QLatin1String("toolchains.xml"))
                || lowerFile.startsWith(QLatin1String("qtversion.xml"))
                || lowerFile.startsWith(QLatin1String("devices.xml"))
                || lowerFile.startsWith(QLatin1String("debuggers.xml"))
                || lowerFile.startsWith(QLatin1String("qtcreator.")))
            QFile::copy(srcDir.absoluteFilePath(file), destDir.absoluteFilePath(file));
        if (file == QLatin1String("qtcreator"))
            copyRecursively(srcDir.absoluteFilePath(file), destDir.absoluteFilePath(file));
    }

    // Make sure to use the copied settings:
    delete settings;
    return createUserSettings();
}

static const char *SHARE_PATH =
        Utils::HostOsInfo::isMacHost() ? "/../Resources" : "/../share/qtcreator";

//OPENMV-DIFF//
void myMessageOutput(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    Q_UNUSED(context)
    QByteArray localMsg = msg.toLocal8Bit();
    switch (type) {
    case QtDebugMsg:
        fprintf(stdout, "%s\n", localMsg.constData()); fflush(stdout);
        break;
    case QtInfoMsg:
        fprintf(stderr, "%s\n", localMsg.constData()); fflush(stderr);
        break;
    case QtWarningMsg:
        if(msg.compare(QLatin1String("JIT is disabled for QML. Property bindings and animations will be "
                                     "very slow. Visit https://wiki.qt.io/V4 to learn about possible "
                                     "solutions for your platform.")) == 0) break;
        fprintf(stderr, "%s\n", localMsg.constData()); fflush(stderr);
        break;
    case QtCriticalMsg:
        fprintf(stderr, "%s\n", localMsg.constData()); fflush(stderr);
        break;
    case QtFatalMsg:
        fprintf(stderr, "%s\n", localMsg.constData()); fflush(stderr);
        abort();
    }
}
//OPENMV-DIFF//
int main(int argc, char **argv)
{
    //OPENMV-DIFF//
    qInstallMessageHandler(myMessageOutput);
    //OPENMV-DIFF//
    const char *highDpiEnvironmentVariable = setHighDpiEnvironmentVariable();

    QLoggingCategory::setFilterRules(QLatin1String("qtc.*.debug=false"));
#ifdef Q_OS_MAC
    // increase the number of file that can be opened in Qt Creator.
    struct rlimit rl;
    getrlimit(RLIMIT_NOFILE, &rl);

    rl.rlim_cur = qMin((rlim_t)OPEN_MAX, rl.rlim_max);
    setrlimit(RLIMIT_NOFILE, &rl);
#endif

    SharedTools::QtSingleApplication app((QLatin1String(appNameC)), argc, argv);

    if (highDpiEnvironmentVariable)
        qunsetenv(highDpiEnvironmentVariable);

    if (Utils::HostOsInfo().isWindowsHost()
            && !qFuzzyCompare(qApp->devicePixelRatio(), 1.0)
            && QApplication::style()->objectName().startsWith(
                QLatin1String("windows"), Qt::CaseInsensitive)) {
        QApplication::setStyle(QLatin1String("fusion"));
    }
    const int threadCount = QThreadPool::globalInstance()->maxThreadCount();
    QThreadPool::globalInstance()->setMaxThreadCount(qMax(4, 2 * threadCount));

    CrashHandlerSetup setupCrashHandler; // Display a backtrace once a serious signal is delivered.

#ifdef ENABLE_QT_BREAKPAD
    QtSystemExceptionHandler systemExceptionHandler;
#endif

    app.setAttribute(Qt::AA_UseHighDpiPixmaps);

    // Manually determine -settingspath command line option
    // We can't use the regular way of the plugin manager, because that needs to parse plugin meta data
    // but the settings path can influence which plugins are enabled
    QString settingsPath;
    QStringList customPluginPaths;
    QStringList arguments = app.arguments(); // adapted arguments list is passed to plugin manager later
    QMutableStringListIterator it(arguments);
    bool testOptionProvided = false;
    while (it.hasNext()) {
        const QString &arg = it.next();
        if (arg == QLatin1String(SETTINGS_OPTION)) {
            it.remove();
            if (it.hasNext()) {
                settingsPath = QDir::fromNativeSeparators(it.next());
                it.remove();
            }
        } else if (arg == QLatin1String(PLUGINPATH_OPTION)) {
            it.remove();
            if (it.hasNext()) {
                customPluginPaths << QDir::fromNativeSeparators(it.next());
                it.remove();
            }
        } else if (arg == QLatin1String(TEST_OPTION)) {
            testOptionProvided = true;
        }
    }
    QScopedPointer<QTemporaryDir> temporaryCleanSettingsDir;
    if (settingsPath.isEmpty() && testOptionProvided) {
        const QString settingsPathTemplate = QDir::cleanPath(QDir::tempPath()
            //OPENMV-DIFF//
            //+ QString::fromLatin1("/qtc-test-settings-XXXXXX"));
            //OPENMV-DIFF//
            + QString::fromLatin1("/openmvide-test-settings-XXXXXX"));
            //OPENMV-DIFF//
        temporaryCleanSettingsDir.reset(new QTemporaryDir(settingsPathTemplate));
        if (!temporaryCleanSettingsDir->isValid())
            return 1;
        settingsPath = temporaryCleanSettingsDir->path();
    }
    if (!settingsPath.isEmpty())
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settingsPath);

    // Must be done before any QSettings class is created
    QSettings::setPath(QSettings::IniFormat, QSettings::SystemScope,
                       QCoreApplication::applicationDirPath() + QLatin1String(SHARE_PATH));
    QSettings::setDefaultFormat(QSettings::IniFormat);
    // plugin manager takes control of this settings object
    QSettings *settings = userSettings();

    QSettings *globalSettings = new QSettings(QSettings::IniFormat, QSettings::SystemScope,
                                              QLatin1String(Core::Constants::IDE_SETTINGSVARIANT_STR),
                                              //OPENMV-DIFF//
                                              //QLatin1String("QtCreator"));
                                              //OPENMV-DIFF//
                                              QLatin1String("OpenMVIDE"));
                                              //OPENMV-DIFF//
    PluginManager pluginManager;
    PluginManager::setPluginIID(QLatin1String("org.qt-project.Qt.QtCreatorPlugin"));
    PluginManager::setGlobalSettings(globalSettings);
    PluginManager::setSettings(settings);

    QTranslator translator;
    QTranslator qtTranslator;
    QStringList uiLanguages;
    uiLanguages = QLocale::system().uiLanguages();
    QString overrideLanguage = settings->value(QLatin1String("General/OverrideLanguage")).toString();
    if (!overrideLanguage.isEmpty())
        uiLanguages.prepend(overrideLanguage);
    const QString &creatorTrPath = QCoreApplication::applicationDirPath()
            + QLatin1String(SHARE_PATH) + QLatin1String("/translations");
    foreach (QString locale, uiLanguages) {
        locale = QLocale(locale).name();
        if (translator.load(QLatin1String("qtcreator_") + locale, creatorTrPath)) {
            const QString &qtTrPath = QLibraryInfo::location(QLibraryInfo::TranslationsPath);
            const QString &qtTrFile = QLatin1String("qt_") + locale;
            // Binary installer puts Qt tr files into creatorTrPath
            if (qtTranslator.load(qtTrFile, qtTrPath) || qtTranslator.load(qtTrFile, creatorTrPath)) {
                app.installTranslator(&translator);
                app.installTranslator(&qtTranslator);
                app.setProperty("qtc_locale", locale);
                break;
            }
            translator.load(QString()); // unload()
        } else if (locale == QLatin1String("C") /* overrideLanguage == "English" */) {
            // use built-in
            break;
        } else if (locale.startsWith(QLatin1String("en")) /* "English" is built-in */) {
            // use built-in
            break;
        }
    }

    // Make sure we honor the system's proxy settings
    QNetworkProxyFactory::setUseSystemConfiguration(true);

    // Load
    const QStringList pluginPaths = getPluginPaths() + customPluginPaths;
    PluginManager::setPluginPaths(pluginPaths);
    QMap<QString, QString> foundAppOptions;
    if (arguments.size() > 1) {
        QMap<QString, bool> appOptions;
        appOptions.insert(QLatin1String(HELP_OPTION1), false);
        appOptions.insert(QLatin1String(HELP_OPTION2), false);
        appOptions.insert(QLatin1String(HELP_OPTION3), false);
        appOptions.insert(QLatin1String(HELP_OPTION4), false);
        appOptions.insert(QLatin1String(VERSION_OPTION), false);
        //OPENMV-DIFF//
        //appOptions.insert(QLatin1String(CLIENT_OPTION), false);
        //appOptions.insert(QLatin1String(PID_OPTION), true);
        //appOptions.insert(QLatin1String(BLOCK_OPTION), false);
        //OPENMV-DIFF//
        QString errorMessage;
        if (!PluginManager::parseOptions(arguments, appOptions, &foundAppOptions, &errorMessage)) {
            displayError(errorMessage);
            printHelp(QFileInfo(app.applicationFilePath()).baseName());
            return -1;
        }
    }

    const PluginSpecSet plugins = PluginManager::plugins();
    PluginSpec *coreplugin = 0;
    //OPENMV-DIFF//
    PluginSpec *texteditorplugin = 0;
    PluginSpec *openmvplugin = 0;
    //OPENMV-DIFF//
    foreach (PluginSpec *spec, plugins) {
        if (spec->name() == QLatin1String(corePluginNameC)) {
            coreplugin = spec;
            //OPENMV-DIFF//
            //break;
            //OPENMV-DIFF//
        }
        //OPENMV-DIFF//
        if (spec->name() == QLatin1String("TextEditor")) {
            texteditorplugin = spec;
        }
        if (spec->name() == QLatin1String("OpenMV")) {
            openmvplugin = spec;
        }
        //OPENMV-DIFF//
    }
    if (!coreplugin) {
        QString nativePaths = QDir::toNativeSeparators(pluginPaths.join(QLatin1Char(',')));
        const QString reason = QCoreApplication::translate("Application", "Could not find Core plugin in %1").arg(nativePaths);
        displayError(msgCoreLoadFailure(reason));
        return 1;
    }
    //OPENMV-DIFF//
    if (!texteditorplugin) {
        QString nativePaths = QDir::toNativeSeparators(pluginPaths.join(QLatin1Char(',')));
        const QString reason = QCoreApplication::translate("Application", "Could not find TextEditor plugin in %1").arg(nativePaths);
        displayError(msgCoreLoadFailure(reason));
        return 1;
    }
    if (!openmvplugin) {
        QString nativePaths = QDir::toNativeSeparators(pluginPaths.join(QLatin1Char(',')));
        const QString reason = QCoreApplication::translate("Application", "Could not find OpenMV plugin in %1").arg(nativePaths);
        displayError(msgCoreLoadFailure(reason));
        return 1;
    }
    //OPENMV-DIFF//
    if (!coreplugin->isEffectivelyEnabled()) {
        const QString reason = QCoreApplication::translate("Application", "Core plugin is disabled.");
        displayError(msgCoreLoadFailure(reason));
        return 1;
    }
    //OPENMV-DIFF//
    if (!texteditorplugin->isEffectivelyEnabled()) {
        const QString reason = QCoreApplication::translate("Application", "TextEditor plugin is disabled.");
        displayError(msgCoreLoadFailure(reason));
        return 1;
    }
    if (!openmvplugin->isEffectivelyEnabled()) {
        const QString reason = QCoreApplication::translate("Application", "OpenMV plugin is disabled.");
        displayError(msgCoreLoadFailure(reason));
        return 1;
    }
    //OPENMV-DIFF//
    if (coreplugin->hasError()) {
        displayError(msgCoreLoadFailure(coreplugin->errorString()));
        return 1;
    }
    //OPENMV-DIFF//
    if (texteditorplugin->hasError()) {
        displayError(msgCoreLoadFailure(texteditorplugin->errorString()));
        return 1;
    }
    if (openmvplugin->hasError()) {
        displayError(msgCoreLoadFailure(openmvplugin->errorString()));
        return 1;
    }
    //OPENMV-DIFF//
    if (foundAppOptions.contains(QLatin1String(VERSION_OPTION))) {
        printVersion(coreplugin);
        return 0;
    }
    if (foundAppOptions.contains(QLatin1String(HELP_OPTION1))
            || foundAppOptions.contains(QLatin1String(HELP_OPTION2))
            || foundAppOptions.contains(QLatin1String(HELP_OPTION3))
            || foundAppOptions.contains(QLatin1String(HELP_OPTION4))) {
        printHelp(QFileInfo(app.applicationFilePath()).baseName());
        return 0;
    }

    qint64 pid = -1;
    //OPENMV-DIFF//
    //if (foundAppOptions.contains(QLatin1String(PID_OPTION))) {
    //    QString pidString = foundAppOptions.value(QLatin1String(PID_OPTION));
    //    bool pidOk;
    //    qint64 tmpPid = pidString.toInt(&pidOk);
    //    if (pidOk)
    //        pid = tmpPid;
    //}
    //OPENMV-DIFF//

    //OPENMV-DIFF//
    //bool isBlock = foundAppOptions.contains(QLatin1String(BLOCK_OPTION));
    //OPENMV-DIFF//
    bool isBlock = false;
    //OPENMV-DIFF//
    if (app.isRunning() && (pid != -1 || isBlock
                            //OPENMV-DIFF//
                            || true
                            //OPENMV-DIFF//
                            || foundAppOptions.contains(QLatin1String(CLIENT_OPTION)))) {
        app.setBlock(isBlock);
        if (app.sendMessage(PluginManager::serializedArguments(), 5000 /*timeout*/, pid))
            return 0;

        // Message could not be send, maybe it was in the process of quitting
        if (app.isRunning(pid)) {
            // Nah app is still running, ask the user
            int button = askMsgSendFailed();
            while (button == QMessageBox::Retry) {
                if (app.sendMessage(PluginManager::serializedArguments(), 5000 /*timeout*/, pid))
                    return 0;
                if (!app.isRunning(pid)) // App quit while we were trying so start a new creator
                    button = QMessageBox::Yes;
                else
                    button = askMsgSendFailed();
            }
            if (button == QMessageBox::No)
                return -1;
        }
    }

    PluginManager::loadPlugins();
    if (coreplugin->hasError()) {
        displayError(msgCoreLoadFailure(coreplugin->errorString()));
        return 1;
    }

    // Set up remote arguments.
    QObject::connect(&app, SIGNAL(messageReceived(QString,QObject*)),
                     &pluginManager, SLOT(remoteArguments(QString,QObject*)));

    QObject::connect(&app, SIGNAL(fileOpenRequest(QString)), coreplugin->plugin(),
                     SLOT(fileOpenRequest(QString)));

    // shutdown plugin manager on the exit
    QObject::connect(&app, SIGNAL(aboutToQuit()), &pluginManager, SLOT(shutdown()));

    //OPENMV-DIFF//
    pluginManager.remoteArguments(PluginManager::serializedArguments(), Q_NULLPTR);
    //OPENMV-DIFF//
    return app.exec();
}
