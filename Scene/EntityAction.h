/**
 *  For conditions of distribution and use, see copyright notice in license.txt
 *
 *  @file   EntityAction.h
 *  @brief  Represent an executable command on an Entity.
 */

#ifndef incl_Scene_EntityAction_h
#define incl_Scene_EntityAction_h

#include "CoreTypes.h"

#include <QObject>

class Entity;

/// Represent an executable command on an Entity.
/** Components (or other instances) can register to these actions by using Entity::ConnectAction().
    Actions allow more complicated in-world logic to be built in slightly more data-driven fashion.
    Actions cannot be created directly, they're created by Entity::RegisterAction(). */
class EntityAction : public QObject
{
    Q_OBJECT
    Q_ENUMS(ExecType)

    friend class Entity;

public:
    /// Destructor.
    ~EntityAction() {}

    /// Returns name of the action.
    QString Name() const { return name; }

    /// Execution type of the action, i.e. where the actions is executed.
    /** As combinations we get local+server, local+peers(all clients but not server),
        server+peers (everyone but me), local+server+peers (everyone).
        Not all of these sound immediately sensible even, but we know we need to be able to do different things at different times.
        Use the ExecTypeField type to store logical OR combinations of execution types. */
    enum ExecType
    {
        Invalid = 0, ///< Invalid.
        Local = 1, ///< Executed locally.
        Server = 2, ///< Executed on server.
        Peers = 4 ///< Executed on peers.
    };

    /// Used to to store logical OR combinations of execution types.
    typedef unsigned int ExecTypeField;

signals:
    /// Emitted when action is triggered.
    /** @param param1 1st parameter for the action, if applicable.
        @param param2 2nd parameter for the action, if applicable.
        @param param3 3rd parameter for the action, if applicable.
        @param params Rest of the parameters, if applicable. */
    void Triggered(QString param1, QString param2, QString param3, QStringList params);

private:
    /// Constructor.
    /** @param name Name of the action. */
    explicit EntityAction(const QString &name);

    /// Triggers this action i.e. emits the Triggered signal.
    /** @param p1 1st parameter for the action, if applicable.
        @param p1 2nd parameter for the action, if applicable.
        @param p1 3rd parameter for the action, if applicable.
        @param p1 Rest of the parameters, if applicable. */
    void Trigger(const QString &p1 = "", const QString &p2 = "", const QString &p3 = "", const QStringList &params = QStringList());

    QString name; ///< Name of the action.
};

#endif
