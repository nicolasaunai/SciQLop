#ifndef SCIQLOP_VARIABLECONTROLLER_H
#define SCIQLOP_VARIABLECONTROLLER_H

#include <Data/SqpDateTime.h>

#include <QLoggingCategory>
#include <QObject>

#include <Common/spimpl.h>

class IDataProvider;
class QItemSelectionModel;
class TimeController;
class Variable;
class VariableModel;

Q_DECLARE_LOGGING_CATEGORY(LOG_VariableController)

/**
 * @brief The VariableController class aims to handle the variables in SciQlop.
 */
class VariableController : public QObject {
    Q_OBJECT
public:
    explicit VariableController(QObject *parent = 0);
    virtual ~VariableController();

    VariableModel *variableModel() noexcept;
    QItemSelectionModel *variableSelectionModel() noexcept;

    void setTimeController(TimeController *timeController) noexcept;


signals:
    /// Signal emitted when a variable has been created
    void variableCreated(std::shared_ptr<Variable> variable);

public slots:
    /// Request the data loading of the variable whithin dateTime
    void onRequestDataLoading(std::shared_ptr<Variable> variable, const SqpDateTime &dateTime);
    /**
     * Creates a new variable and adds it to the model
     * @param name the name of the new variable
     * @param provider the data provider for the new variable
     */
    void createVariable(const QString &name, std::shared_ptr<IDataProvider> provider) noexcept;

    /// Update the temporal parameters of every selected variable to dateTime
    void onDateTimeOnSelection(const SqpDateTime &dateTime);

    void initialize();
    void finalize();

private:
    void waitForFinish();

    class VariableControllerPrivate;
    spimpl::unique_impl_ptr<VariableControllerPrivate> impl;
};

#endif // SCIQLOP_VARIABLECONTROLLER_H
