/* ----------------------------------------------------------------------------

 * GTSAM Copyright 2010, Georgia Tech Research Corporation, 
 * Atlanta, Georgia 30332-0415
 * All Rights Reserved
 * Authors: Frank Dellaert, et al. (see THANKS for the full author list)

 * See LICENSE for the license information

 * -------------------------------------------------------------------------- */

/**
 * @file    BayesTree
 * @brief   Bayes Tree is a tree of cliques of a Bayes Chain
 * @author  Frank Dellaert
 */

// \callgraph

#pragma once

#include <vector>
#include <stdexcept>
#include <deque>
#include <boost/function.hpp>
#include <boost/shared_ptr.hpp>

#include <gtsam/base/types.h>
#include <gtsam/base/FastVector.h>
#include <gtsam/inference/FactorGraph.h>
#include <gtsam/inference/BayesNet.h>
#include <gtsam/linear/VectorValues.h>

namespace gtsam {

	/**
	 * Bayes tree
	 * Templated on the CONDITIONAL class, the type of node in the underlying Bayes chain.
	 * This could be a ConditionalProbabilityTable, a GaussianConditional, or a SymbolicConditional
	 *
	 * \ingroup Multifrontal
	 */
	template<class CONDITIONAL>
	class BayesTree {

	public:

	  typedef boost::shared_ptr<BayesTree<CONDITIONAL> > shared_ptr;
		typedef boost::shared_ptr<CONDITIONAL> sharedConditional;
		typedef boost::shared_ptr<BayesNet<CONDITIONAL> > sharedBayesNet;
		typedef CONDITIONAL ConditionalType;
		typedef typename CONDITIONAL::FactorType FactorType;
	  typedef typename FactorGraph<FactorType>::Eliminate Eliminate;

		/**
		 * A Clique in the tree is an incomplete Bayes net: the variables
		 * in the Bayes net are the frontal nodes, and the variables conditioned
		 * on are the separator. We also have pointers up and down the tree.
		 *
		 * Since our Conditional class already handles multiple frontal variables,
		 * this Clique contains exactly 1 conditional.
		 */
		struct Clique {

		protected:
		  void assertInvariants() const;

		public:
		  typedef CONDITIONAL ConditionalType;
			typedef typename boost::shared_ptr<Clique> shared_ptr;
			typedef typename boost::weak_ptr<Clique> weak_ptr;
			sharedConditional conditional_;
			weak_ptr parent_;
			std::list<shared_ptr> children_;
			typename FactorType::shared_ptr cachedFactor_;

			friend class BayesTree<CONDITIONAL>;

			//* Constructor */
			Clique() {}

			Clique(const sharedConditional& conditional);

			void cloneToBayesTree(BayesTree& newTree, shared_ptr parent_clique = shared_ptr()) const {
			  sharedConditional newConditional = sharedConditional(new CONDITIONAL(*conditional_));
			  sharedClique newClique = newTree.addClique(newConditional, parent_clique);
			  if (cachedFactor_)
			    newClique->cachedFactor_ = cachedFactor_->clone();
			  else newClique->cachedFactor_ = typename FactorType::shared_ptr();
			  if (!parent_clique) {
			    newTree.root_ = newClique;
			  }
			  BOOST_FOREACH(const shared_ptr& childClique, children_) {
			    childClique->cloneToBayesTree(newTree, newClique);
			  }
			}

			/** print this node */
			void print(const std::string& s = "") const;

			/** The arrow operator accesses the conditional */
			const CONDITIONAL* operator->() const { return conditional_.get(); }

			/** The arrow operator accesses the conditional */
			CONDITIONAL* operator->() { return conditional_.get(); }

			/** Access the conditional */
			const sharedConditional& conditional() const { return conditional_; }

			/** is this the root of a Bayes tree ? */
			inline bool isRoot() const { return parent_.expired(); }

			/** return the const reference of children */
			std::list<shared_ptr>& children() { return children_; }
			const std::list<shared_ptr>& children() const { return children_; }

			/** The size of subtree rooted at this clique, i.e., nr of Cliques */
			size_t treeSize() const;

			/** Access the cached factor (this is a hack) */
			typename FactorType::shared_ptr& cachedFactor() { return cachedFactor_; }

			/** print this node and entire subtree below it */
			void printTree(const std::string& indent="") const;

			/** Permute the variables in the whole subtree rooted at this clique */
			void permuteWithInverse(const Permutation& inversePermutation);

			/** Permute variables when they only appear in the separators.  In this
			 * case the running intersection property will be used to prevent always
			 * traversing the whole tree.  Returns whether any separator variables in
			 * this subtree were reordered.
			 */
			bool permuteSeparatorWithInverse(const Permutation& inversePermutation);

			/** return the conditional P(S|Root) on the separator given the root */
			// TODO: create a cached version
			BayesNet<CONDITIONAL> shortcut(shared_ptr root, Eliminate function);

			/** return the marginal P(C) of the clique */
			FactorGraph<FactorType> marginal(shared_ptr root, Eliminate function);

			/** return the joint P(C1,C2), where C1==this. TODO: not a method? */
			FactorGraph<FactorType> joint(shared_ptr C2, shared_ptr root, Eliminate function);

			bool equals(const Clique& other, double tol=1e-9) const {
			  return (!conditional_ && !other.conditional()) ||
			  		conditional_->equals(*(other.conditional()), tol);
			}

	  private:
	    /** Serialization function */
	    friend class boost::serialization::access;
	    template<class ARCHIVE>
	    void serialize(ARCHIVE & ar, const unsigned int version) {
	    	ar & BOOST_SERIALIZATION_NVP(conditional_);
	    	ar & BOOST_SERIALIZATION_NVP(parent_);
	    	ar & BOOST_SERIALIZATION_NVP(children_);
	    	ar & BOOST_SERIALIZATION_NVP(cachedFactor_);
	    }

		}; // \struct Clique

		// typedef for shared pointers to cliques
		typedef boost::shared_ptr<Clique> sharedClique;

		// A convenience class for a list of shared cliques
		struct Cliques : public std::list<sharedClique> {
			void print(const std::string& s = "Cliques") const;
			bool equals(const Cliques& other, double tol = 1e-9) const;
		};

		/** clique statistics */
		struct CliqueStats {
			double avgConditionalSize;
			std::size_t maxConditionalSize;
			double avgSeparatorSize;
			std::size_t maxSeparatorSize;
		};

		/** store all the sizes  */
		struct CliqueData {
			std::vector<std::size_t> conditionalSizes;
			std::vector<std::size_t> separatorSizes;
			CliqueStats getStats() const;
		};

    /** Map from keys to Clique */
    typedef std::deque<sharedClique> Nodes;

	protected:

    /** Map from keys to Clique */
		Nodes nodes_;

		/** private helper method for saving the Tree to a text file in GraphViz format */
		void saveGraph(std::ostream &s, sharedClique clique,
				int parentnum = 0) const;

		/** Gather data on a single clique */
		void getCliqueData(CliqueData& stats, sharedClique clique) const;

	protected:

		/** Root clique */
		sharedClique root_;

		/** remove a clique: warning, can result in a forest */
		void removeClique(sharedClique clique);

		/** add a clique (top down) */
		sharedClique addClique(const sharedConditional& conditional,
				sharedClique parent_clique = sharedClique());

		/** add a clique (bottom up) */
		sharedClique addClique(const sharedConditional& conditional,
				std::list<sharedClique>& child_cliques);

		/**
		 * Add a conditional to the front of a clique, i.e. a conditional whose
		 * parents are already in the clique or its separators.  This function does
		 * not check for this condition, it just updates the data structures.
		 */
		static void addToCliqueFront(BayesTree<CONDITIONAL>& bayesTree,
		    const sharedConditional& conditional, const sharedClique& clique);

		/** Fill the nodes index for a subtree */
		void fillNodesIndex(const sharedClique& subtree);

	public:

		/** Create an empty Bayes Tree */
		BayesTree();

		/** Create a Bayes Tree from a Bayes Net */
		BayesTree(const BayesNet<CONDITIONAL>& bayesNet);

		/**
		 * Create a Bayes Tree from a Bayes Net and some subtrees. The Bayes net corresponds to the
		 * new root clique and the subtrees are connected to the root clique.
		 */
		BayesTree(const BayesNet<CONDITIONAL>& bayesNet, std::list<BayesTree<CONDITIONAL> > subtrees);

		/** Destructor */
		virtual ~BayesTree() {}

		/**
		 * Constructing Bayes trees
		 */

		/** Insert a new conditional
		 * This function only applies for Symbolic case with IndexCondtional,
		 * We make it static so that it won't be compiled in GaussianConditional case.
		 * */
		static void insert(BayesTree<CONDITIONAL>& bayesTree, const sharedConditional& conditional);

		/**
		 * Insert a new clique corresponding to the given Bayes net.
		 * It is the caller's responsibility to decide whether the given Bayes net is a valid clique,
		 * i.e. all the variables (frontal and separator) are connected
		 */
		sharedClique insert(const sharedConditional& clique,
				std::list<sharedClique>& children, bool isRootClique = false);

		/**
		 * Hang a new subtree off of the existing tree.  This finds the appropriate
		 * parent clique for the subtree (which may be the root), and updates the
		 * nodes index with the new cliques in the subtree.  None of the frontal
		 * variables in the subtree may appear in the separators of the existing
		 * BayesTree.
		 */
		void insert(const sharedClique& subtree);

		/**
		 * Querying Bayes trees
		 */

		/** check equality */
		bool equals(const BayesTree<CONDITIONAL>& other, double tol = 1e-9) const;

		/** deep copy from another tree */
		void cloneTo(shared_ptr& newTree) const {
		  root_->cloneToBayesTree(*newTree);
		}

		/**
		 * Find parent clique of a conditional.  It will look at all parents and
		 * return the one with the lowest index in the ordering.
		 */
		template<class CONTAINER>
		Index findParentClique(const CONTAINER& parents) const;

		/** number of cliques */
		inline size_t size() const {
			if(root_)
				return root_->treeSize();
			else
				return 0;
		}

    /** return nodes */
    Nodes nodes() const { return nodes_; }

		/** return root clique */
		const sharedClique& root() const { return root_;	}
		sharedClique& root() { return root_; }

		/** find the clique to which key belongs */
		sharedClique operator[](Index key) const {
			return nodes_.at(key);
		}

		/** Gather data on all cliques */
		CliqueData getCliqueData() const;

		/** return marginal on any variable */
		typename FactorType::shared_ptr marginalFactor(Index key,
				Eliminate function) const;

		/**
		 * return marginal on any variable, as a Bayes Net
		 * NOTE: this function calls marginal, and then eliminates it into a Bayes Net
		 * This is more expensive than the above function
		 */
		typename BayesNet<CONDITIONAL>::shared_ptr marginalBayesNet(Index key,
				Eliminate function) const;

		/** return joint on two variables */
		typename FactorGraph<FactorType>::shared_ptr joint(Index key1, Index key2,
				Eliminate function) const;

		/** return joint on two variables as a BayesNet */
		typename BayesNet<CONDITIONAL>::shared_ptr jointBayesNet(Index key1,
				Index key2, Eliminate function) const;

		/**
		 * Read only with side effects
		 */

		/** print */
		void print(const std::string& s = "") const;

		/** saves the Tree to a text file in GraphViz format */
		void saveGraph(const std::string& s) const;

		/**
		 * Altering Bayes trees
		 */

		/** Remove all nodes */
		void clear();

		/**
		 * Remove path from clique to root and return that path as factors
		 * plus a list of orphaned subtree roots. Used in removeTop below.
		 */
		void removePath(sharedClique clique, BayesNet<CONDITIONAL>& bn, Cliques& orphans);

		/**
		 * Given a list of keys, turn "contaminated" part of the tree back into a factor graph.
		 * Factors and orphans are added to the in/out arguments.
		 */
		template<class CONTAINER>
		void removeTop(const CONTAINER& keys, BayesNet<CONDITIONAL>& bn, Cliques& orphans);

  private:
    /** Serialization function */
    friend class boost::serialization::access;
    template<class ARCHIVE>
    void serialize(ARCHIVE & ar, const unsigned int version) {
    	ar & BOOST_SERIALIZATION_NVP(nodes_);
    	ar & BOOST_SERIALIZATION_NVP(root_);
    }

	}; // BayesTree


  /* ************************************************************************* */
  template<class CONDITIONAL>
  void _BayesTree_dim_adder(
      std::vector<size_t>& dims,
      const typename BayesTree<CONDITIONAL>::sharedClique& clique) {

    if(clique) {
      // Add dims from this clique
      for(typename CONDITIONAL::const_iterator it = (*clique)->beginFrontals(); it != (*clique)->endFrontals(); ++it)
        dims[*it] = (*clique)->dim(it);

      // Traverse children
      BOOST_FOREACH(const typename BayesTree<CONDITIONAL>::sharedClique& child, clique->children()) {
        _BayesTree_dim_adder<CONDITIONAL>(dims, child);
      }
    }
  }

	/* ************************************************************************* */
	template<class CONDITIONAL>
	boost::shared_ptr<VectorValues> allocateVectorValues(const BayesTree<CONDITIONAL>& bt) {
	  std::vector<size_t> dimensions(bt.nodes().size(), 0);
	  _BayesTree_dim_adder<CONDITIONAL>(dimensions, bt.root());
	  return boost::shared_ptr<VectorValues>(new VectorValues(dimensions));
	}

} /// namespace gtsam
