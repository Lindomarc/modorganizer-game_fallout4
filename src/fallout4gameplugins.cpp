#include "fallout4gameplugins.h"
#include <safewritefile.h>
#include <report.h>
#include <ipluginlist.h>
#include <report.h>
#include <scopeguard.h>

#include <QDir>
#include <QTextCodec>
#include <QStringList>


using MOBase::IPluginGame;
using MOBase::IPluginList;
using MOBase::IOrganizer;
using MOBase::SafeWriteFile;
using MOBase::reportError;

static const std::set<QString> OFFICIAL_FILES{
    "fallout4.esm", "dlcrobot.esm", "dlcworkshop01.esm", "dlccoast.esm"};

Fallout4GamePlugins::Fallout4GamePlugins(IOrganizer *organizer)
  : GamebryoGamePlugins(organizer)
{
}

void Fallout4GamePlugins::writePluginList(const IPluginList *pluginList,
                                          const QString &filePath) {
  SafeWriteFile file(filePath);

  QTextCodec *textCodec = localCodec();

  file->resize(0);

  file->write(textCodec->fromUnicode(
      "# This file was automatically generated by Mod Organizer.\r\n"));

  bool invalidFileNames = false;
  int writtenCount = 0;

  QStringList plugins = pluginList->pluginNames();
  std::sort(plugins.begin(), plugins.end(),
            [pluginList](const QString &lhs, const QString &rhs) {
              return pluginList->priority(lhs) < pluginList->priority(rhs);
            });

  for (const QString &pluginName : plugins) {
    if (pluginList->state(pluginName) == IPluginList::STATE_ACTIVE) {
      if (!textCodec->canEncode(pluginName)) {
        invalidFileNames = true;
        qCritical("invalid plugin name %s", qPrintable(pluginName));
      } else {
        file->write("*");
        file->write(textCodec->fromUnicode(pluginName));
      }
      file->write("\r\n");
      ++writtenCount;
    }
  }

  if (invalidFileNames) {
    reportError(QObject::tr("Some of your plugins have invalid names! These "
                            "plugins can not be loaded by the game. Please see "
                            "mo_interface.log for a list of affected plugins "
                            "and rename them."));
  }

  if (file.commitIfDifferent(m_LastSaveHash[filePath])) {
    qDebug("%s saved", qPrintable(QDir::toNativeSeparators(filePath)));
  }
}

bool Fallout4GamePlugins::readPluginList(MOBase::IPluginList *pluginList,
                                         const QString &filePath,
                                         bool useLoadOrder)
{
  QStringList plugins = pluginList->pluginNames();

  for (const QString &pluginName : OFFICIAL_FILES) {
    if (pluginList->state(pluginName) != IPluginList::STATE_MISSING) {
      pluginList->setState(pluginName, IPluginList::STATE_ACTIVE);
    }
  }

  QFile file(filePath);
  if (!file.open(QIODevice::ReadOnly)) {
    qWarning("%s not found", qPrintable(filePath));
    return false;
  }
  ON_BLOCK_EXIT([&]() { file.close(); });

  if (file.size() == 0) {
    // MO stores at least a header in the file. if it's completely empty the
    // file is broken
    qWarning("%s empty", qPrintable(filePath));
    return false;
  }

  QStringList loadOrder = organizer()->managedGame()->primaryPlugins();
  while (!file.atEnd()) {
    QByteArray line = file.readLine();
    QString pluginName;
    if ((line.size() > 0) && (line.at(0) != '#')) {
      pluginName = localCodec()->toUnicode(line.trimmed().constData());
    }
    if (pluginName.startsWith('*')) {
      pluginName.remove(0, 1);
    }
    if (pluginName.size() > 0) {
      pluginList->setState(pluginName, IPluginList::STATE_ACTIVE);
      plugins.removeAll(pluginName);
      if (!loadOrder.contains(pluginName, Qt::CaseInsensitive)) {
        loadOrder.append(pluginName);
      }
    }
  }

  file.close();

  // we removed each plugin found in the file, so what's left are inactive mods
  for (const QString &pluginName : plugins) {
    pluginList->setState(pluginName, IPluginList::STATE_INACTIVE);
  }

  if (useLoadOrder) {
    pluginList->setLoadOrder(loadOrder);
  }

  return true;
}
