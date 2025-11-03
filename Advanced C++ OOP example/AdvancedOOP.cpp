// AdvancedOOP.cpp
// Compile with: g++ -std=c++17 AdvancedOOP.cpp -pthread -O2
// or MSVC with appropriate flags. Example demonstrates many OOP patterns.

#include <iostream>
#include <string>
#include <memory>
#include <vector>
#include <unordered_map>
#include <functional>
#include <mutex>
#include <atomic>
#include <typeindex>
#include <type_traits>
#include <sstream>
#include <utility>

// ----------------------------- Utilities ----------------------------------

// Simple debug logging utility
struct Logger {
    static void log(const std::string& s) { std::cout << s << std::endl; }
};

// Helper: string join for diagnostics
template<typename It>
std::string join(It begin, It end, const std::string& sep = ", ") {
    std::ostringstream os;
    if (begin == end) return "";
    os << *begin++;
    for (; begin != end; ++begin) os << sep << *begin;
    return os.str();
}

// ------------------------- Type Erasure (Command) -------------------------

// Generic Command pattern via type-erased callable; shows runtime polymorphism.
// The class stores any callable with signature void().
class Command {
    struct ICommand {
        virtual ~ICommand() = default;
        virtual void invoke() = 0;
        virtual std::unique_ptr<ICommand> clone() const = 0;
    };

    template<typename F>
    struct Model : ICommand {
        F f;
        // accept rvalues (move)
        Model(F&& fn) : f(std::forward<F>(fn)) {}
        // accept lvalues (copy)
        Model(const F& fn) : f(fn) {}
        void invoke() override { f(); }
        std::unique_ptr<ICommand> clone() const override { return std::make_unique<Model<F>>(f); }
    };

    std::unique_ptr<ICommand> ptr;

public:
    Command() = default;
    template<typename F, typename = typename std::enable_if<!std::is_same<typename std::decay<F>::type, Command>::value>::type>
    Command(F&& f) : ptr(std::make_unique<Model<F>>(std::forward<F>(f))) {}
    Command(const Command& other) : ptr(other.ptr ? other.ptr->clone() : nullptr) {}
    Command(Command&&) noexcept = default;
    Command& operator=(const Command& o) { ptr = o.ptr ? o.ptr->clone() : nullptr; return *this; }
    Command& operator=(Command&&) noexcept = default;

    void operator()() const { if (ptr) ptr->invoke(); }
    explicit operator bool() const { return static_cast<bool>(ptr); }
};

// ----------------------------- Entity System --------------------------------

// Forward declaration for component
class IComponent;

// Base interface for Entities exposing polymorphic lifecycle
class IEntity {
public:
    virtual ~IEntity() = default;
    virtual std::string name() const = 0;
    virtual void update(double dt) = 0;
    virtual void accept(class IEntityVisitor& v) = 0;
};

// Visitor interface for Entities (runtime polymorphic visitor)
class IEntityVisitor {
public:
    virtual ~IEntityVisitor() = default;
    virtual void visit(IEntity& e) = 0;
};

// ------------------------ Component and Mixins -----------------------------

// A base component interface demonstrating composition over inheritance.
class IComponent {
public:
    virtual ~IComponent() = default;
    virtual std::string type() const = 0;
    virtual void tick(double dt) = 0;
};

// CRTP mixin to give components a name + common behavior without virtual overhead in derived final types
template<typename Derived>
class NamedComponent {
public:
    template<typename S>
    void setName(S&& s) { name_ = std::forward<S>(s); }
    const std::string& getName() const { return name_; }

protected:
    std::string name_;
};

// Example concrete component using CRTP mixin and virtual interface
class PhysicsComponent : public IComponent, public NamedComponent<PhysicsComponent> {
public:
    PhysicsComponent(double mass = 1.0) : mass_(mass), velocity_{0.0} {}
    std::string type() const override { return "Physics"; }
    void tick(double dt) override {
        // Simple integration; illustrate use of component state
        position_ += velocity_ * dt;
        // tiny damping
        velocity_ *= 0.999;
    }
    void applyImpulse(double v) { velocity_ += v / mass_; }
    double mass() const { return mass_; }
    double position() const { return position_; }

private:
    double mass_;
    double velocity_;
    double position_{0.0};
};

// Another component: scripting-like callback container (type-erased)
class ScriptComponent : public IComponent {
public:
    ScriptComponent() = default;
    std::string type() const override { return "Script"; }
    void tick(double dt) override {
        // Run all registered scripts for this tick.
        for (auto &cb : callbacks_) if (cb) cb(dt);
    }
    void registerCallback(std::function<void(double)> cb) { callbacks_.push_back(std::move(cb)); }
private:
    std::vector<std::function<void(double)>> callbacks_;
};

// -------------------------- Policy-based Entity ----------------------------

// Policy to control memory strategy: default uses std::unique_ptr; custom allocator could be injected.
// Make Owner accept templates with extra parameters
// Unary ownership policy: compatible with template-template parameters
template<typename T>
struct OwnershipPolicy {
    using owner_type = std::unique_ptr<T>;
    static owner_type create(T* raw) { return owner_type(raw); }
};

// Logging policy to attach logging to lifecycle events
struct LoggingPolicy {
    static void onCreate(const std::string& nm) { Logger::log("[Create] " + nm); }
    static void onDestroy(const std::string& nm) { Logger::log("[Destroy] " + nm); }
};

// Combine policies via inheritance (empty base optimization will minimize overhead)
template<typename Derived,
    template<typename...> class Ownership = OwnershipPolicy,
    typename LogPolicy = LoggingPolicy>
    class EntityBase : public IEntity {    
public:
    using comp_owner = typename Ownership<IComponent>::owner_type;
    EntityBase(std::string name) : name_(std::move(name)) { LogPolicy::onCreate(name_); }
    virtual ~EntityBase() { LogPolicy::onDestroy(name_); }

    std::string name() const override { return name_; }

    // Manage components via type-indexed registry
    template<typename Comp, typename... Args>
    void addComponent(Args&&... args) {
        static_assert(std::is_base_of<IComponent, Comp>::value, "Comp must implement IComponent");
        auto comp = Ownership<IComponent>::template create(new Comp(std::forward<Args>(args)...));
        std::type_index k(typeid(Comp));
        std::lock_guard<std::mutex> lg(mutex_);
        components_[k] = std::move(comp);
    }

    template<typename Comp>
    Comp* getComponent() {
        std::type_index k(typeid(Comp));
        std::lock_guard<std::mutex> lg(mutex_);
        auto it = components_.find(k);
        if (it == components_.end()) return nullptr;
        return dynamic_cast<Comp*>(it->second.get());
    }

    // Default update loops over components
    void update(double dt) override {
        std::vector<IComponent*> snapshot;
        {
            std::lock_guard<std::mutex> lg(mutex_);
            snapshot.reserve(components_.size());
            for (auto &p : components_) snapshot.push_back(p.second.get());
        }
        for (auto* comp : snapshot) comp->tick(dt);
    }

    // Visitor acceptance — runtime polymorphism hook
    void accept(IEntityVisitor& v) override { v.visit(*this); }

private:
    std::string name_;
    std::unordered_map<std::type_index, comp_owner> components_;
    mutable std::mutex mutex_;
};

// ---------------------- Pimpl + Factory + Registry -------------------------

// Pimpl for a heavyweight controller object to hide implementation details
class Controller {
public:
    Controller(std::string id);
    ~Controller();
    void command(const Command& c);
    std::string id() const;

private:
    struct Impl;
    std::unique_ptr<Impl> pimpl; // Pimpl pointer hides implementation, improves compilation boundaries
};

// Implementation (kept local to translation unit)
struct Controller::Impl {
    explicit Impl(std::string i) : id(std::move(i)) {}
    std::string id;
    std::vector<Command> history;
    std::mutex lock;
    void push(Command c) {
        std::lock_guard<std::mutex> lg(lock);
        history.push_back(c);
        if (c) c(); // execute immediately for demo
    }
};

Controller::Controller(std::string id) : pimpl(std::make_unique<Impl>(std::move(id))) {}
Controller::~Controller() = default;
void Controller::command(const Command& c) { pimpl->push(c); }
std::string Controller::id() const { return pimpl->id; }

// A Registry for runtime-registered entity factories (plugin-style)
class EntityFactoryRegistry {
public:
    using FactoryFn = std::function<std::unique_ptr<IEntity>(const std::string& name)>;

    static EntityFactoryRegistry& instance() {
        static EntityFactoryRegistry reg;
        return reg;
    }

    void registerFactory(const std::string& type, FactoryFn f) {
        std::lock_guard<std::mutex> lg(mutex_);
        factories_[type] = std::move(f);
    }

    std::unique_ptr<IEntity> create(const std::string& type, const std::string& name) {
        std::lock_guard<std::mutex> lg(mutex_);
        auto it = factories_.find(type);
        if (it == factories_.end()) return nullptr;
        return it->second(name);
    }

    std::vector<std::string> list() {
        std::lock_guard<std::mutex> lg(mutex_);
        std::vector<std::string> out; out.reserve(factories_.size());
        for (auto &p : factories_) out.push_back(p.first);
        return out;
    }

private:
    std::unordered_map<std::string, FactoryFn> factories_;
    std::mutex mutex_;
};

// --------------------- Concrete Entity: GameEntity --------------------------

// Compose EntityBase with policies: unique ownership; logging enabled
class GameEntity : public EntityBase<GameEntity> {
public:
    GameEntity(std::string name) : EntityBase(std::move(name)) {}
    // Could add game-specific behavior
    void accept(IEntityVisitor& v) override { v.visit(*this); } // specialized acceptance
};

// Register factory for GameEntity at static initialization to demonstrate registry usage
namespace {
    const bool registeredGameEntity = (EntityFactoryRegistry::instance().registerFactory(
        "GameEntity",
        [](const std::string& name){ return std::make_unique<GameEntity>(name); }
    ), true);
}

// --------------------------- Visitor Example --------------------------------

// A concrete visitor that inspects Entities and prints component types
class IntrospectVisitor : public IEntityVisitor {
public:
    void visit(IEntity& e) override {
        // Try dynamic_cast to GameEntity to access its API; demonstrates safe RTTI usage
        std::ostringstream os;
        os << "Introspecting: " << e.name();
        Logger::log(os.str());
        // If it's an EntityBase, we could expose internal components via dynamic cast:
        if (auto ge = dynamic_cast<GameEntity*>(&e)) {
            // Example: ask for PhysicsComponent if present
            if (auto pc = ge->getComponent<PhysicsComponent>()) {
                std::ostringstream os2;
                os2 << "  Physics mass=" << pc->mass() << " pos=" << pc->position();
                Logger::log(os2.str());
            }
            if (auto sc = ge->getComponent<ScriptComponent>()) {
                Logger::log("  Has ScriptComponent with callbacks");
            }
        }
    }
};

// ---------------------------- Thread Safety Demo ----------------------------

// A tiny thread-safe manager of Entities showing shared_ptr/weak_ptr and atomic control.
class Scene {
public:
    void add(std::shared_ptr<IEntity> e) {
        std::lock_guard<std::mutex> lg(mutex_);
        entities_.push_back(e);
    }

    // Iterate safe snapshot
    void updateAll(double dt) {
        std::vector<std::shared_ptr<IEntity>> snap;
        {
            std::lock_guard<std::mutex> lg(mutex_);
            snap = entities_;
        }
        for (auto &ent : snap) if (ent) ent->update(dt);
    }

    std::vector<std::string> names() const {
        std::lock_guard<std::mutex> lg(mutex_);
        std::vector<std::string> out;
        for (auto &e : entities_) {
            if (e) out.push_back(e->name());
            else out.push_back("<expired>");
        }
        return out;
    }

private:
    std::vector<std::shared_ptr<IEntity>> entities_;
    mutable std::mutex mutex_;
};

// ------------------------------- Demo usage ---------------------------------

int main() {
    // Create a controller (Pimpl hides heavy internals)
    Controller controller("main");

    // Create a scene and add an entity created via the registry (runtime factory)
    Scene scene;
    auto entPtr = EntityFactoryRegistry::instance().create("GameEntity", "Player");
    auto sharedEnt = std::shared_ptr<IEntity>(std::move(entPtr)); // transfer to shared_ptr
    // Add components via GameEntity API (requires dynamic_cast)
    if (auto ge = std::dynamic_pointer_cast<GameEntity>(sharedEnt)) {
        ge->addComponent<PhysicsComponent>(75.0); // mass
        ge->addComponent<ScriptComponent>();
        if (auto sc = ge->getComponent<ScriptComponent>()) {
            // Register a lambda callback that captures ge weakly to avoid cycle
            std::weak_ptr<GameEntity> weakGe = std::dynamic_pointer_cast<GameEntity>(sharedEnt);
            sc->registerCallback([weakGe](double dt){
                if (auto locked = weakGe.lock()) {
                    // This callback demonstrates safe use of weak_ptr in closures
                    Logger::log("Script callback running for " + locked->name());
                }
            });
        }
    }

    // Add to scene
    scene.add(sharedEnt);

    // Register a command that mutates physics component via type safe lookup
    controller.command(Command([shared = sharedEnt](){
        if (auto ge = std::dynamic_pointer_cast<GameEntity>(shared)) {
            if (auto pc = ge->getComponent<PhysicsComponent>()) {
                pc->applyImpulse(400.0); // big impulse
                Logger::log("Applied impulse to " + ge->name());
            }
        }
    }));

    // Use visitor to introspect entities
    IntrospectVisitor visitor;
    if (sharedEnt) sharedEnt->accept(visitor);

    // Simulation loop (small, single-threaded demo)
    for (int i = 0; i < 3; ++i) {
        scene.updateAll(0.016); // assume 60 FPS dt
    }

    // Show scene contents
    auto names = scene.names();
    Logger::log(std::string("Scene contains: ") + join(names.begin(), names.end()));

    // Demonstrate type erasure storing heterogeneous handlers
    std::vector<Command> deferred;
    deferred.emplace_back([&](){ Logger::log("Deferred: save game state for " + sharedEnt->name()); });
    deferred.emplace_back([&](){ Logger::log("Deferred: cleanup temporary resources"); });

    // Execute deferred commands
    for (auto &c : deferred) if (c) c();

    // Not portable; spawns command interpreter
    system("pause");
    return 0;
}
