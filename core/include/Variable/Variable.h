#ifndef SCIQLOP_VARIABLE_H
#define SCIQLOP_VARIABLE_H

#include <Common/spimpl.h>

class IDataSeries;
class QString;

/**
 * @brief The Variable class represents a variable in SciQlop.
 */
class Variable {
public:
    explicit Variable(const QString &name, const QString &unit, const QString &mission);

    QString name() const noexcept;
    QString mission() const noexcept;
    QString unit() const noexcept;

    void addDataSeries(std::unique_ptr<IDataSeries> dataSeries) noexcept;

private:
    class VariablePrivate;
    spimpl::unique_impl_ptr<VariablePrivate> impl;
};

#endif // SCIQLOP_VARIABLE_H
