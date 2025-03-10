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

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "icontext.h"
#include "icore.h"

#include <utils/appmainwindow.h>
#include <utils/dropsupport.h>

#include <QMap>
#include <QColor>

#include <functional>

QT_BEGIN_NAMESPACE
class QSettings;
class QPrinter;
class QToolButton;
QT_END_NAMESPACE

namespace Core {

class StatusBarWidget;
class EditorManager;
class ExternalToolManager;
class DocumentManager;
class HelpManager;
class IDocument;
class IWizardFactory;
class JsExpander;
class MessageManager;
class ModeManager;
class ProgressManager;
class NavigationWidget;
class RightPaneWidget;
class SettingsDatabase;
class VcsManager;

namespace Internal {

class FancyTabWidget;
class GeneralSettings;
class ProgressManagerPrivate;
class ShortcutSettings;
class ToolSettings;
class MimeTypeSettings;
class StatusBarManager;
class VersionDialog;
class WindowSupport;
class SystemEditor;
class SystemSettings;

//OPENMV-DIFF//
class CORE_EXPORT MainWindow : public Utils::AppMainWindow
//OPENMV-DIFF//
//class MainWindow : public Utils::AppMainWindow
//OPENMV-DIFF//
{
    Q_OBJECT

public:
    MainWindow();
    ~MainWindow();

    bool init(QString *errorMessage);
    void extensionsInitialized();
    void aboutToShutdown();

    IContext *contextObject(QWidget *widget);
    void addContextObject(IContext *context);
    void removeContextObject(IContext *context);

    IDocument *openFiles(const QStringList &fileNames,
                         ICore::OpenFilesFlags flags,
                         const QString &workingDirectory = QString());

    inline SettingsDatabase *settingsDatabase() const { return m_settingsDatabase; }
    virtual QPrinter *printer() const;
    IContext * currentContextObject() const;
    QStatusBar *statusBar() const;

    void updateAdditionalContexts(const Context &remove, const Context &add,
                                  ICore::ContextPriority priority);

    void setSuppressNavigationWidget(bool suppress);

    void setOverrideColor(const QColor &color);

    QStringList additionalAboutInformation() const;
    void appendAboutInformation(const QString &line);

    void addPreCloseListener(const std::function<bool()> &listener);
    //OPENMV-DIFF//
    void disableShow(bool disable) { m_disableShow = disable; }
    bool isShowDisabled() const { return m_disableShow; }
    //OPENMV-DIFF//

signals:
    //OPENMV-DIFF//
    void showEventSignal();
    void hideEventSignal();
    //OPENMV-DIFF//
    void newItemDialogRunningChanged();

public slots:
    void openFileWith();
    void exit();

    bool showOptionsDialog(Id page = Id(), QWidget *parent = 0);

    bool showWarningWithOptions(const QString &title, const QString &text,
                                const QString &details = QString(),
                                Id settingsId = Id(),
                                QWidget *parent = 0);

protected:
    //OPENMV-DIFF//
    virtual void showEvent(QShowEvent *event) { emit showEventSignal(); Utils::AppMainWindow::showEvent(event); }
    virtual void hideEvent(QHideEvent *event) { emit hideEventSignal(); Utils::AppMainWindow::hideEvent(event); }
    //OPENMV-DIFF//
    virtual void closeEvent(QCloseEvent *event);

private:
    void openFile();
    void aboutToShowRecentFiles();
    void setFocusToEditor();
    void saveAll();
    void aboutQtCreator();
    void aboutPlugins();
    void updateFocusWidget(QWidget *old, QWidget *now);
    void setSidebarVisible(bool visible);
    void destroyVersionDialog();
    void openDroppedFiles(const QList<Utils::DropSupport::FileSpec> &files);
    void restoreWindowState();
    void newItemDialogFinished();

    void updateContextObject(const QList<IContext *> &context);
    void updateContext();

    void registerDefaultContainers();
    void registerDefaultActions();

    void readSettings();
    void writeSettings();

    ICore *m_coreImpl;
    QStringList m_aboutInformation;
    Context m_highPrioAdditionalContexts;
    Context m_lowPrioAdditionalContexts;
    SettingsDatabase *m_settingsDatabase;
    mutable QPrinter *m_printer;
    WindowSupport *m_windowSupport;
    EditorManager *m_editorManager;
    ExternalToolManager *m_externalToolManager;
    MessageManager *m_messageManager;
    ProgressManagerPrivate *m_progressManager;
    JsExpander *m_jsExpander;
    VcsManager *m_vcsManager;
    StatusBarManager *m_statusBarManager;
    ModeManager *m_modeManager;
    //OPENMV-DIFF//
    //HelpManager *m_helpManager;
    //OPENMV-DIFF//
    FancyTabWidget *m_modeStack;
    NavigationWidget *m_navigationWidget;
    RightPaneWidget *m_rightPaneWidget;
    StatusBarWidget *m_outputView;
    VersionDialog *m_versionDialog;
    //OPENMV-DIFF//
    bool m_disableShow;
    //OPENMV-DIFF//

    QList<IContext *> m_activeContext;

    QMap<QWidget *, IContext *> m_contextWidgets;

    GeneralSettings *m_generalSettings;
    SystemSettings *m_systemSettings;
    ShortcutSettings *m_shortcutSettings;
    ToolSettings *m_toolSettings;
    MimeTypeSettings *m_mimeTypeSettings;
    SystemEditor *m_systemEditor;

    // actions
    QAction *m_focusToEditor;
    QAction *m_newAction;
    QAction *m_openAction;
    QAction *m_openWithAction;
    QAction *m_saveAllAction;
    QAction *m_exitAction;
    QAction *m_optionsAction;
    QAction *m_toggleSideBarAction;
    QAction *m_toggleModeSelectorAction;
    QAction *m_themeAction;

    QToolButton *m_toggleSideBarButton;
    QColor m_overrideColor;
    QList<std::function<bool()>> m_preCloseListeners;
};

} // namespace Internal
} // namespace Core

#endif // MAINWINDOW_H
