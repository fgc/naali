/**
 *  For conditions of distribution and use, see copyright notice in license.txt
 *
 *  @file   ConsoleAPI.cpp
 *  @brief  Exposes debug console functionality to scripting languages.
 *
 *          Allows printing text to console, executing console commands programmatically
 *          and registering new console commands.
 *
 *  @note   Currently just simple wrapper class but the idea is to refactor
 *          whole console system later on.
 */

#include "StableHeaders.h"
#include "DebugOperatorNew.h"
#include "ConsoleAPI.h"
#include "Framework.h"
#include "ConsoleServiceInterface.h"
#include "ConsoleCommandServiceInterface.h"
#include "MemoryLeakCheck.h"

#include "LoggingFunctions.h"
//DEFINE_POCO_LOGGING_FUNCTIONS("ConsoleAPI")

void Command::Invoke(const QStringList &params)
{
    emit Invoked(params);
}

ConsoleAPI::~ConsoleAPI()
{
    qDeleteAll(commands_);
}

Command *ConsoleAPI::RegisterCommand(const QString &name, const QString &desc)
{
    Command *command = 0;
    Console::ConsoleCommandServiceInterface *consoleCommand = framework_->GetService<Console::ConsoleCommandServiceInterface>();
    if (consoleCommand)
    {
        if(commands_.contains(name))
        {
            LogWarning("Command " + name.toStdString() + " is already registered.");
            return commands_[name];
        }

        command = new Command(name);
        commands_.insert(name, command);

        Console::Command cmd = { name.toStdString(), desc.toStdString(), Console::CallbackPtr(), false };
        consoleCommand->RegisterCommand(cmd);

        // Use UniqueConnection so that we don't have duplicate connections.
        connect(consoleCommand, SIGNAL(CommandInvoked(const QString &, const QStringList &)),
            SLOT(CheckForCommand(const QString &, const QStringList &)), Qt::UniqueConnection);
    }

    return command;
}

void ConsoleAPI::RegisterCommand(const QString &name, const QString &desc, const QObject *receiver, const char *member)
{
    Console::ConsoleCommandServiceInterface *consoleCommand = framework_->GetService<Console::ConsoleCommandServiceInterface>();
    if (consoleCommand && !commands_.contains(name))
    {
        Command *command = new Command(name);
        commands_.insert(name, command);
        connect(command, SIGNAL(Invoked(const QStringList &)), receiver, member);

        Console::Command cmd = { name.toStdString(), desc.toStdString(), Console::CallbackPtr(), false };
        consoleCommand->RegisterCommand(cmd);

        // Use UniqueConnection so that we don't have duplicate connections.
        connect(consoleCommand, SIGNAL(CommandInvoked(const QString &, const QStringList &)),
            SLOT(CheckForCommand(const QString &, const QStringList &)), Qt::UniqueConnection);
    }
}

void ConsoleAPI::ExecuteCommand(const QString &command)
{
    Console::ConsoleServiceInterface *console = framework_->GetService<Console::ConsoleServiceInterface>();
    if (console)
        console->ExecuteCommand(command.toStdString());
}

void ConsoleAPI::Print(const QString &message)
{
    Console::ConsoleServiceInterface *console = framework_->GetService<Console::ConsoleServiceInterface>();
    if (console)
        console->Print(message.toStdString());
}

ConsoleAPI::ConsoleAPI(Foundation::Framework *fw) :
    QObject(fw),
    framework_(fw)
{
}

void ConsoleAPI::CheckForCommand(const QString &name, const QStringList &params) const
{
    QMap<QString, Command *>::const_iterator i = commands_.find(name);
    if (i != commands_.end())
        i.value()->Invoke(params);
}

