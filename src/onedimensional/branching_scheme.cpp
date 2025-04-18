#include "onedimensional/branching_scheme.hpp"

using namespace packingsolver;
using namespace packingsolver::onedimensional;

////////////////////////////////////////////////////////////////////////////////
/////////////////////////////// BranchingScheme ////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

BranchingScheme::BranchingScheme(
        const Instance& instance,
        const Parameters& parameters):
    instance_(instance),
    parameters_(parameters)
{
}

////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////// children ///////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

BranchingScheme::Node BranchingScheme::child_tmp(
        const std::shared_ptr<Node>& pparent,
        const Insertion& insertion) const
{
    const Node& parent = *pparent;
    Node node;
    const ItemType& item_type = instance().item_type(insertion.item_type_id);

    node.parent = pparent;

    node.item_type_id = insertion.item_type_id;

    // Update number_of_bins and last_bin_direction.
    if (insertion.new_bin) {  // New bin.
        node.number_of_bins = parent.number_of_bins + 1;
        node.last_bin_length = item_type.length;
        node.last_bin_weight = item_type.weight;
        node.last_bin_number_of_items = 1;
        node.last_bin_weight = item_type.weight;
        node.last_bin_maximum_number_of_items = item_type.maximum_stackability;
        node.last_bin_remaining_weight = item_type.maximum_weight_after;
    } else {  // Same bin.
        node.number_of_bins = parent.number_of_bins;
        node.last_bin_length = parent.last_bin_length + item_type.length - item_type.nesting_length;
        node.last_bin_weight = parent.last_bin_weight + item_type.weight;
        node.last_bin_number_of_items = parent.last_bin_number_of_items + 1;
        node.last_bin_weight = parent.last_bin_weight + item_type.weight;
        node.last_bin_maximum_number_of_items = std::min(
                parent.last_bin_maximum_number_of_items,
                item_type.maximum_stackability);
        node.last_bin_remaining_weight = std::min(
                parent.last_bin_remaining_weight - item_type.weight,
                item_type.maximum_weight_after);
    }

    BinPos i = node.number_of_bins - 1;

    // Compute item_number_of_copies, number_of_items, items_area,
    // squared_item_length and profit.
    node.item_number_of_copies = parent.item_number_of_copies;
    node.item_number_of_copies[insertion.item_type_id]++;
    node.number_of_items = parent.number_of_items + 1;
    node.item_length = parent.item_length + item_type.length;
    node.squared_item_length = parent.squared_item_length + item_type.length * item_type.length;
    node.profit = parent.profit + item_type.profit;

    // Compute current_length, guide_length and width using uncovered_items.
    node.current_length = instance_.previous_bin_length(i) + node.last_bin_length;
    node.waste = node.current_length - node.item_length;

    node.id = node_id_++;
    return node;
}

std::vector<std::shared_ptr<BranchingScheme::Node>> BranchingScheme::children(
        const std::shared_ptr<Node>& parent) const
{
    insertions(parent);
    std::vector<std::shared_ptr<Node>> cs(insertions_.size());
    for (Counter i = 0; i < (Counter)insertions_.size(); ++i)
        cs[i] = std::make_shared<Node>(child_tmp(parent, insertions_[i]));
    return cs;
}

const std::vector<BranchingScheme::Insertion>& BranchingScheme::insertions(
        const std::shared_ptr<Node>& parent) const
{
    //std::cout << "insertions" << std::endl;

    insertions_.clear();

    // Insert an item in the same bin.
    if (parent->number_of_bins > 0) {
        BinTypeId bin_type_id = instance().bin_type_id(parent->number_of_bins - 1);
        const BinType& bin_type = instance().bin_type(bin_type_id);
        for (ItemTypeId item_type_id: bin_type.item_type_ids) {
            const ItemType& item_type = instance_.item_type(item_type_id);
            if (parent->item_number_of_copies[item_type_id] == item_type.copies)
                continue;
            insertion_item_same_bin(parent, item_type_id);
        }
    }

    // Insert in a new bin.
    if (insertions_.empty() && parent->number_of_bins < instance().number_of_bins()) {
        BinTypeId bin_type_id = instance().bin_type_id(parent->number_of_bins);
        const BinType& bin_type = instance().bin_type(bin_type_id);
        for (ItemTypeId item_type_id: bin_type.item_type_ids) {
            const ItemType& item_type = instance_.item_type(item_type_id);
            if (parent->item_number_of_copies[item_type_id] == item_type.copies)
                continue;
            insertion_item_new_bin(parent, item_type_id);
        }
    }

    return insertions_;
}

void BranchingScheme::insertion_item_same_bin(
        const std::shared_ptr<Node>& parent,
        ItemTypeId item_type_id) const
{
    const ItemType& item_type = instance_.item_type(item_type_id);
    BinTypeId bin_type_id = instance().bin_type_id(parent->number_of_bins - 1);
    const BinType& bin_type = instance().bin_type(bin_type_id);

    // Check bin length.
    if (parent->last_bin_length + item_type.length - item_type.nesting_length > bin_type.length)
        return;
    // Check maximum weight.
    if (parent->last_bin_weight + item_type.weight > bin_type.maximum_weight * PSTOL)
        return;
    // Check maximum stackability.
    ItemPos last_bin_maximum_number_of_items = std::min(
            parent->last_bin_maximum_number_of_items,
            item_type.maximum_stackability);
    if (parent->last_bin_number_of_items + 1 > last_bin_maximum_number_of_items)
        return;
    // Check maximum weight above.
    if (item_type.weight > parent->last_bin_remaining_weight * PSTOL)
        return;

    Insertion insertion;
    insertion.item_type_id = item_type_id;
    insertion.new_bin = false;
    insertions_.push_back(insertion);
}

void BranchingScheme::insertion_item_new_bin(
        const std::shared_ptr<Node>& parent,
        ItemTypeId item_type_id) const
{
    //std::cout << "insertion_item " << item_type_id << std::endl;
    const ItemType& item_type = instance_.item_type(item_type_id);
    BinTypeId bin_type_id = instance().bin_type_id(parent->number_of_bins);
    const BinType& bin_type = instance().bin_type(bin_type_id);
    // Check bin length.
    if (item_type.length > bin_type.length)
        return;
    // Check maximum weight.
    if (item_type.weight > bin_type.maximum_weight * PSTOL)
        return;

    Insertion insertion;
    insertion.item_type_id = item_type_id;
    insertion.new_bin = true;
    insertions_.push_back(insertion);
}

bool BranchingScheme::dominates(
        const std::shared_ptr<Node>& node_1,
        const std::shared_ptr<Node>& node_2) const
{
    if (node_1->item_type_id != node_2->item_type_id)
        return false;
    return node_1->current_length <= node_2->current_length;
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

const std::shared_ptr<BranchingScheme::Node> BranchingScheme::root() const
{
    BranchingScheme::Node node;
    node.item_number_of_copies = std::vector<ItemPos>(instance_.number_of_item_types(), 0);
    node.id = node_id_++;
    return std::make_shared<Node>(node);
}

bool BranchingScheme::better(
        const std::shared_ptr<Node>& node_1,
        const std::shared_ptr<Node>& node_2) const
{
    switch (instance_.objective()) {
    case Objective::Default: {
        if (node_2->profit > node_1->profit)
            return false;
        if (node_2->profit < node_1->profit)
            return true;
        return node_2->waste > node_1->waste;
    } case Objective::BinPacking: case Objective::VariableSizedBinPacking: {
        if (!leaf(node_1))
            return false;
        if (!leaf(node_2))
            return true;
        return node_2->number_of_bins > node_1->number_of_bins;
    } case Objective::BinPackingWithLeftovers: {
        if (!leaf(node_1))
            return false;
        if (!leaf(node_2))
            return true;
        return node_2->waste > node_1->waste;
    } case Objective::Knapsack: {
        return node_2->profit < node_1->profit;
    } default: {
        std::stringstream ss;
        ss << "Branching scheme 'onedimensional::BranchingScheme'"
            << "does not support objective '" << instance_.objective() << "'.";
        throw std::logic_error(ss.str());
        return false;
    }
    }
}

bool BranchingScheme::bound(
        const std::shared_ptr<Node>& node_1,
        const std::shared_ptr<Node>& node_2) const
{
    switch (instance_.objective()) {
    case Objective::Default: {
        if (!leaf(node_2)) {
            return (ubkp(*node_1) <= node_2->profit);
        } else {
            if (ubkp(*node_1) != node_2->profit)
                return (ubkp(*node_1) <= node_2->profit);
            return node_1->waste >= node_2->waste;
        }
    } case Objective::BinPacking: case Objective::VariableSizedBinPacking: {
        if (!leaf(node_2))
            return false;
        BinPos bin_pos = -1;
        Area a = instance_.item_length() + node_1->waste;
        while (a > 0) {
            bin_pos++;
            if (bin_pos >= instance_.number_of_bins())
                return true;
            BinTypeId bin_type_id = instance().bin_type_id(bin_pos);
            a -= instance().bin_type(bin_type_id).length;
        }
        return (bin_pos + 1 >= node_2->number_of_bins);
    } case Objective::BinPackingWithLeftovers: {
        if (!leaf(node_2))
            return false;
        return node_1->waste >= node_2->waste;
    } case Objective::Knapsack: {
        return false;
        return (ubkp(*node_1) <= node_2->profit);
    } default: {
        std::stringstream ss;
        ss << "Branching scheme 'onedimensional::BranchingScheme'"
            << "does not support objective '" << instance_.objective() << "'.";
        throw std::logic_error(ss.str());
        return false;
    }
    }
}

////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////// export ////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

Solution BranchingScheme::to_solution(
        const std::shared_ptr<Node>& node) const
{
    std::vector<std::shared_ptr<Node>> descendents;
    for (std::shared_ptr<Node> current_node = node;
            current_node->parent != nullptr;
            current_node = current_node->parent) {
        descendents.push_back(current_node);
    }
    std::reverse(descendents.begin(), descendents.end());

    Solution solution(instance());
    BinPos bin_pos = -1;
    for (auto current_node: descendents) {
        if (current_node->number_of_bins > solution.number_of_bins())
            bin_pos = solution.add_bin(instance().bin_type_id(current_node->number_of_bins - 1), 1);
        solution.add_item(bin_pos, current_node->item_type_id);
    }
    return solution;
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

bool BranchingScheme::Insertion::operator==(
        const Insertion& insertion) const
{
    return ((item_type_id == insertion.item_type_id)
            && (new_bin == insertion.new_bin)
            );
}

std::ostream& packingsolver::onedimensional::operator<<(
        std::ostream& os,
        const BranchingScheme::Insertion& insertion)
{
    os << "item_type_id " << insertion.item_type_id
        << " new_bin " << insertion.new_bin
        ;
    return os;
}

std::ostream& packingsolver::onedimensional::operator<<(
        std::ostream& os,
        const BranchingScheme::Node& node)
{
    os << "number_of_items " << node.number_of_items
        << " number_of_bins " << node.number_of_bins
        << std::endl;
    os << "item_length " << node.item_length
        << " current_length " << node.current_length
        << std::endl;
    os << "waste " << node.waste
        << " profit " << node.profit
        << std::endl;

    // item_number_of_copies
    os << "item_number_of_copies" << std::flush;
    for (ItemPos j_pos: node.item_number_of_copies)
        os << " " << j_pos;
    os << std::endl;

    return os;
}

