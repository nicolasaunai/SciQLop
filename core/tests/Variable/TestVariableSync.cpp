#include <QObject>
#include <QtTest>

#include <memory>

#include <Data/DataProviderParameters.h>
#include <Data/IDataProvider.h>
#include <Data/ScalarSeries.h>
#include <Time/TimeController.h>
#include <Variable/Variable.h>
#include <Variable/VariableController.h>
#include <Variable/VariableModel.h>

namespace {

/// Delay after each operation on the variable before validating it (in ms)
const auto OPERATION_DELAY = 100;

/**
 * Generates values according to a range. The value generated for a time t is the number of seconds
 * of difference between t and a reference value (which is midnight -> 00:00:00)
 *
 * Example: For a range between 00:00:10 and 00:00:20, the generated values are
 * {10,11,12,13,14,15,16,17,18,19,20}
 */
std::vector<double> values(const SqpRange &range)
{
    QTime referenceTime{0, 0};

    std::vector<double> result{};

    for (auto i = range.m_TStart; i <= range.m_TEnd; ++i) {
        auto time = DateUtils::dateTime(i).time();
        result.push_back(referenceTime.secsTo(time));
    }

    return result;
}

/// Provider used for the tests
class TestProvider : public IDataProvider {
    std::shared_ptr<IDataProvider> clone() const { return std::make_shared<TestProvider>(); }

    void requestDataLoading(QUuid acqIdentifier, const DataProviderParameters &parameters) override
    {
        const auto &ranges = parameters.m_Times;

        for (const auto &range : ranges) {
            // Generates data series
            auto valuesData = values(range);

            std::vector<double> xAxisData{};
            for (auto i = range.m_TStart; i <= range.m_TEnd; ++i) {
                xAxisData.push_back(i);
            }

            auto dataSeries = std::make_shared<ScalarSeries>(
                std::move(xAxisData), std::move(valuesData), Unit{"t", true}, Unit{});

            emit dataProvided(acqIdentifier, dataSeries, range);
        }
    }

    void requestDataAborting(QUuid acqIdentifier) override
    {
        // Does nothing
    }
};

/**
 * Interface representing an operation performed on a variable controller.
 * This interface is used in tests to apply a set of operations and check the status of the
 * controller after each operation
 */
struct IOperation {
    virtual ~IOperation() = default;
    /// Executes the operation on the variable controller
    virtual void exec(VariableController &variableController) const = 0;
};

/**
 *Variable creation operation in the controller
 */
struct Create : public IOperation {
    explicit Create(int index) : m_Index{index} {}

    void exec(VariableController &variableController) const override
    {
        auto variable = variableController.createVariable(QString::number(m_Index), {},
                                                          std::make_unique<TestProvider>());
    }

    int m_Index; ///< The index of the variable to create in the controller
};

/**
 * Variable move/shift operation in the controller
 */
struct Move : public IOperation {
    explicit Move(int index, const SqpRange &newRange, bool shift = false)
            : m_Index{index}, m_NewRange{newRange}, m_Shift{shift}
    {
    }

    void exec(VariableController &variableController) const override
    {
        if (auto variable = variableController.variableModel()->variable(m_Index)) {
            variableController.onRequestDataLoading({variable}, m_NewRange, variable->range(),
                                                    !m_Shift);
        }
    }

    int m_Index;         ///< The index of the variable to move
    SqpRange m_NewRange; ///< The new range of the variable
    bool m_Shift;        ///< Performs a shift (
};

/**
 * Variable synchronization/desynchronization operation in the controller
 */
struct Synchronize : public IOperation {
    explicit Synchronize(int index, QUuid syncId, bool synchronize = true)
            : m_Index{index}, m_SyncId{syncId}, m_Synchronize{synchronize}
    {
    }

    void exec(VariableController &variableController) const override
    {
        if (auto variable = variableController.variableModel()->variable(m_Index)) {
            if (m_Synchronize) {
                variableController.onAddSynchronized(variable, m_SyncId);
            }
            else {
                variableController.desynchronize(variable, m_SyncId);
            }
        }
    }

    int m_Index;        ///< The index of the variable to sync/desync
    QUuid m_SyncId;     ///< The synchronization group of the variable
    bool m_Synchronize; ///< Performs sync or desync operation
};

/**
 * Test Iteration
 *
 * A test iteration includes an operation to be performed, and a set of expected ranges after each
 * operation. Each range is tested after the operation to ensure that:
 * - the range of the variable is the expected range
 * - the data of the variable are those generated for the expected range
 */
struct Iteration {
    std::shared_ptr<IOperation> m_Operation;  ///< Operation to perform
    std::map<int, SqpRange> m_ExpectedRanges; ///< Expected ranges (by variable index)
};

using Iterations = std::vector<Iteration>;

} // namespace

Q_DECLARE_METATYPE(Iterations)

class TestVariableSync : public QObject {
    Q_OBJECT

private slots:
    /// Input data for @sa testSync()
    void testSync_data();

    /// Tests synchronization between variables through several operations
    void testSync();
};

void TestVariableSync::testSync_data()
{
    // ////////////// //
    // Test structure //
    // ////////////// //

    QTest::addColumn<QUuid>("syncId");
    QTest::addColumn<SqpRange>("initialRange");
    QTest::addColumn<Iterations>("iterations");

    // ////////// //
    // Test cases //
    // ////////// //

    // Id used to synchronize variables in the controller
    auto syncId = QUuid::createUuid();

    /// Generates a range according to a start time and a end time (the date is the same)
    auto range = [](const QTime &startTime, const QTime &endTime) {
        return SqpRange{DateUtils::secondsSinceEpoch(QDateTime{{2017, 1, 1}, startTime, Qt::UTC}),
                        DateUtils::secondsSinceEpoch(QDateTime{{2017, 1, 1}, endTime, Qt::UTC})};
    };

    auto initialRange = range({12, 0}, {13, 0});

    Iterations iterations{};
    // Creates variables var0, var1 and var2
    iterations.push_back({std::make_shared<Create>(0), {{0, initialRange}}});
    iterations.push_back({std::make_shared<Create>(1), {{0, initialRange}, {1, initialRange}}});
    iterations.push_back(
        {std::make_shared<Create>(2), {{0, initialRange}, {1, initialRange}, {2, initialRange}}});

    // Adds variables into the sync group (ranges don't need to be tested here)
    iterations.push_back({std::make_shared<Synchronize>(0, syncId)});
    iterations.push_back({std::make_shared<Synchronize>(1, syncId)});
    iterations.push_back({std::make_shared<Synchronize>(2, syncId)});

    // Moves var0: ranges of var0, var1 and var2 change
    auto newRange = range({12, 30}, {13, 30});
    iterations.push_back(
        {std::make_shared<Move>(0, newRange), {{0, newRange}, {1, newRange}, {2, newRange}}});

    // Moves var1: ranges of var0, var1 and var2 change
    newRange = range({13, 0}, {14, 0});
    iterations.push_back(
        {std::make_shared<Move>(0, newRange), {{0, newRange}, {1, newRange}, {2, newRange}}});

    // Moves var2: ranges of var0, var1 and var2 change
    newRange = range({13, 30}, {14, 30});
    iterations.push_back(
        {std::make_shared<Move>(0, newRange), {{0, newRange}, {1, newRange}, {2, newRange}}});

    // Desyncs var2 and moves var0:
    // - ranges of var0 and var1 change
    // - range of var2 doesn't change anymore
    auto var2Range = newRange;
    newRange = range({13, 45}, {14, 45});
    iterations.push_back({std::make_shared<Synchronize>(2, syncId, false)});
    iterations.push_back(
        {std::make_shared<Move>(0, newRange), {{0, newRange}, {1, newRange}, {2, var2Range}}});

    // Shifts var0: although var1 is synchronized with var0, its range doesn't change
    auto var1Range = newRange;
    newRange = range({14, 45}, {15, 45});
    iterations.push_back({std::make_shared<Move>(0, newRange, true),
                          {{0, newRange}, {1, var1Range}, {2, var2Range}}});

    // Moves var0 through several operations:
    // - range of var0 changes
    // - range or var1 changes according to the previous shift (one hour)
    auto moveVar0 = [&iterations](const auto &var0NewRange, const auto &var1ExpectedRange) {
        iterations.push_back(
            {std::make_shared<Move>(0, var0NewRange), {{0, var0NewRange}, {1, var1ExpectedRange}}});
    };
    // Pan left
    moveVar0(range({14, 30}, {15, 30}), range({13, 30}, {14, 30}));
    // Pan right
    moveVar0(range({16, 0}, {17, 0}), range({15, 0}, {16, 0}));
    // Zoom in
    moveVar0(range({16, 30}, {16, 45}), range({15, 30}, {15, 45}));
    // Zoom out
    moveVar0(range({12, 0}, {18, 0}), range({11, 0}, {17, 0}));

    QTest::newRow("sync1") << syncId << initialRange << std::move(iterations);
}

void TestVariableSync::testSync()
{
    // Inits controllers
    TimeController timeController{};
    VariableController variableController{};
    variableController.setTimeController(&timeController);

    QFETCH(QUuid, syncId);
    QFETCH(SqpRange, initialRange);
    timeController.onTimeToUpdate(initialRange);

    // Synchronization group used
    variableController.onAddSynchronizationGroupId(syncId);

    // For each iteration:
    // - execute operation
    // - compare the variables' state to the expected states
    QFETCH(Iterations, iterations);
    for (const auto &iteration : iterations) {
        iteration.m_Operation->exec(variableController);
        QTest::qWait(OPERATION_DELAY);

        for (const auto &expectedRangeEntry : iteration.m_ExpectedRanges) {
            auto variableIndex = expectedRangeEntry.first;
            auto expectedRange = expectedRangeEntry.second;

            // Gets the variable in the controller
            auto variable = variableController.variableModel()->variable(variableIndex);

            // Compares variable's range to the expected range
            QVERIFY(variable != nullptr);
            auto range = variable->range();
            QCOMPARE(range, expectedRange);

            // Compares variable's data with values expected for its range
            auto dataSeries = variable->dataSeries();
            QVERIFY(dataSeries != nullptr);

            auto it = dataSeries->xAxisRange(range.m_TStart, range.m_TEnd);
            auto expectedValues = values(range);
            QVERIFY(std::equal(it.first, it.second, expectedValues.cbegin(), expectedValues.cend(),
                               [](const auto &dataSeriesIt, const auto &expectedValue) {
                                   return dataSeriesIt.value() == expectedValue;
                               }));
        }
    }
}

QTEST_MAIN(TestVariableSync)

#include "TestVariableSync.moc"