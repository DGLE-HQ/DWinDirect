/**
\author		Alexey Shaydurov aka ASH
\date		05.11.2017 (c)Korotkov Andrey

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

#pragma once

#include "stdafx.h"
#include "world hierarchy.h"
#include "frustum culling.h"
#include "occlusion query shceduling.h"

// thread pool based MSVC's std::async implementation can lead to deadlocks during tree traverse, alternative technique needed
#define MULTITHREADED_TREE_TRAVERSE 0

namespace Renderer::Impl::Hierarchy
{
	template<class AABB>
	inline bool AABBSizeSeparator<AABB>::operator ()(const AABB &aabb) const
	{
		// '>=' required (not '>') to handle degenerate AABBs correctly
		return all(aabb.Size() >= size);
	}

	template<Axis axis>
	template<class AABB>
	inline bool AAABBSplitter<axis>::operator ()(const AABB &aabb) const
	{
		constexpr auto axisIdx = std::underlying_type_t<Axis>(axis);
		return aabb.Center()[axisIdx] < split;
	}

	template<Axis axis>
	template<class AABB>
	inline bool AAABBInternalSplitter<axis>::operator ()(const AABB &aabb) const
	{
		constexpr auto axisIdx = std::underlying_type_t<Axis>(axis);
		return aabb.min[axisIdx] < split && aabb.max[axisIdx] > split;
	}

	template<Axis axis>
	template<class AABB>
	inline void AABBAxisBounds<axis>::operator ()(const AABB &aabb) const
	{
		constexpr auto axisIdx = std::underlying_type_t<Axis>(axis);
		min = fmin(min, aabb.min[axisIdx]);
		max = fmax(max, aabb.max[axisIdx]);
	}

	template<class AABBHandler>
	template<class Object>
	inline auto Object2AABB<AABBHandler>::operator()(const Object &object) const
	{
		return AABBHandler::operator ()(object.GetAABB());
	}

	template<class Object, class CustomNodeData, TreeStructure treeStructure>
	template<std::remove_extent_t<decltype(childrenOrder)> ...idx>
	inline BVH<Object, CustomNodeData, treeStructure>::Node::Node(std::integer_sequence<std::remove_extent_t<decltype(childrenOrder)>, idx...>, typename std::enable_if_t<true, decltype(objects)>::iterator srcBegin, typename std::enable_if_t<true, decltype(objects)>::iterator srcEnd) :
		childrenOrder{idx...}, objBegin(srcBegin), objEnd(srcEnd)
	{}

	template<class Object, class CustomNodeData, TreeStructure treeStructure>
	BVH<Object, CustomNodeData, treeStructure>::Node::Node(typename std::enable_if_t<true, decltype(objects)>::iterator srcBegin, typename std::enable_if_t<true, decltype(objects)>::iterator srcEnd, SplitTechnique splitTechnique, ...) :
		Node(std::make_integer_sequence<std::remove_extent_t<decltype(childrenOrder)>, std::extent_v<decltype(childrenOrder)>>(), srcBegin, srcEnd)
	{
		using namespace std;

		assert(srcBegin != srcEnd);

		// calculate AABB and mean pos\
			consider calculating mean pos after big objects being separated
		decltype(aabb.Center()) meanPoint{};
		for_each(srcBegin, srcEnd, [&](const Object &object)
		{
			const auto &curAABB = object.GetAABB();
			aabb.Refit(curAABB);
			meanPoint += curAABB.Center();
		});
		meanPoint /= distance(srcBegin, srcEnd);

		// place big objects into current node
		objExclusiveSeparator = srcBegin = partition(srcBegin, srcEnd, Object2AABB<AABBSizeSeparator<decltype(aabb)>>(aabb.Size()));

		// try to split if feasible
		if (distance(srcBegin, srcEnd) > 1)
		{
			const auto splitPoint = [&, splitTechnique]
			{
				switch (splitTechnique)
				{
				case SplitTechnique::REGULAR:
					return aabb.Center();
				case SplitTechnique::MEAN:
					return meanPoint;
				}
			}();

			bool splitted = false;
			// consider using C++17 constexpr if
			switch (treeStructure)
			{
			case ENNEATREE:
				SplitEneaTree(splitted, srcBegin, srcEnd, splitTechnique, splitPoint);
				break;
			case ICOSEPTREE:
				SplitIcoseptree(splitted, srcBegin, srcEnd, splitTechnique, splitPoint);
				break;
			case QUADTREE:
			case OCTREE:
				va_list params;
				va_start(params, splitTechnique);
				const double overlapThreshold = va_arg(params, double);
				va_end(params);
				switch (treeStructure)
				{
				case QUADTREE:
					SplitQuadtree(splitted, srcBegin, srcEnd, splitTechnique, splitPoint, overlapThreshold);
					break;
				case OCTREE:
					SplitOctree(splitted, srcBegin, srcEnd, splitTechnique, splitPoint, overlapThreshold);
					break;
				}
				break;
			}

			if (splitted)
				childrenCount = distance(begin(children), remove(begin(children), end(children), nullptr));
			else
				objExclusiveSeparator = objEnd;
		}

		// calculate tri count and occlusion
		{
			const auto accumulated = accumulate(objBegin, objExclusiveSeparator, make_pair(0ul, 0.f), [renormalizationFactor = 1.f / aabb.Measure()](auto left, const Object &right)
			{
				left.first += right.GetTriCount();

				// renormalize occlusion and perform increment
				const float renormalizedOcclusionIncrement = right.GetOcclusion() * right.GetAABB().Measure() * renormalizationFactor;
				left.second += fma(-left.second, renormalizedOcclusionIncrement, renormalizedOcclusionIncrement);

				return left;
			});
			exclusiveTriCount = accumulated.first;
			occlusion = accumulated.second;

			inclusiveTriCount = accumulate(cbegin(children), next(cbegin(children), childrenCount), exclusiveTriCount, [](unsigned long int left, const remove_extent_t<decltype(children)> &right) noexcept
			{
				return left + right->GetInclusiveTriCount();
			});

			assert(inclusiveTriCount);
		}
	}

	template<class Object, class CustomNodeData, TreeStructure treeStructure>
	template<typename ...Params>
	inline void BVH<Object, CustomNodeData, treeStructure>::Node::CreateChildNode(bool splitted, typename std::enable_if_t<true, decltype(objects)>::iterator begin, typename std::enable_if_t<true, decltype(objects)>::iterator end, SplitTechnique splitTechnique, unsigned int idxOffset, Params ...params)
	{
		if (splitted)
			children[idxOffset] = make_unique<Node>(begin, end, splitTechnique, params...);
	}

	template<class Object, class CustomNodeData, TreeStructure treeStructure>
	template<Axis axis, class F>
	inline void BVH<Object, CustomNodeData, treeStructure>::Node::Split2(const F &action, bool &splitted, typename std::enable_if_t<true, decltype(objects)>::iterator begin, typename std::enable_if_t<true, decltype(objects)>::iterator end, decltype(aabb.Center()) splitPoint, double overlapThreshold, unsigned int idxOffset)
	{
		using namespace std;

		constexpr auto axisIdx = underlying_type_t<Axis>(axis);
		constexpr unsigned int idxOffsetStride = 1u << axisIdx;

		auto split = partition(begin, end, Object2AABB<AAABBSplitter<axis>>(splitPoint[axisIdx]));
		if (overlapThreshold < 1.)
		{
			float min[2] = { +INFINITY, +INFINITY }, max[2] = { -INFINITY, -INFINITY };
			for_each(begin, split, Object2AABB<AABBAxisBounds<axis>>(min[0], max[0]));
			for_each(split, end, Object2AABB<AABBAxisBounds<axis>>(min[1], max[1]));
			if ((max[0] - min[1]) / fmin(max[0] - min[0], max[1] - min[1]) > overlapThreshold)
				split = end;
		}
		splitted |= split != begin && split != end;
		if (split != begin)
			action(splitted, begin, split, overlapThreshold, idxOffset);
		if (split != end)
			action(splitted, split, end, overlapThreshold, idxOffset + idxOffsetStride);
	}

	template<class Object, class CustomNodeData, TreeStructure treeStructure>
	inline void BVH<Object, CustomNodeData, treeStructure>::Node::SplitQuadtree(bool &splitted, typename std::enable_if_t<true, decltype(objects)>::iterator begin, typename std::enable_if_t<true, decltype(objects)>::iterator end, SplitTechnique splitTechnique, decltype(aabb.Center()) splitPoint, double overlapThreshold, unsigned int idxOffset)
	{
		using namespace std;
		using namespace placeholders;

		const auto createChildNode = bind(&Node::CreateChildNode<double>, this, _1/*splitted*/, _2/*begin*/, _3/*end*/, splitTechnique, _5/*idxOffset*/, _4/*overlapThreshold*/);
		Split2<Axis::Y>(bind(&Node::Split2<Axis::X, decltype(cref(createChildNode))>, this, cref(createChildNode), _1/*slpitted*/, _2/*begin*/, _3/*end*/, splitPoint, _4/*overlapThreshold*/, _5/*idxOffset*/), splitted, begin, end, splitPoint, overlapThreshold, idxOffset);
	}

	// 1 call site
	template<class Object, class CustomNodeData, TreeStructure treeStructure>
	inline void BVH<Object, CustomNodeData, treeStructure>::Node::SplitOctree(bool &splitted, typename std::enable_if_t<true, decltype(objects)>::iterator begin, typename std::enable_if_t<true, decltype(objects)>::iterator end, SplitTechnique splitTechnique, decltype(aabb.Center()) splitPoint, double overlapThreshold)
	{
		using namespace std;
		using namespace placeholders;

		Split2<Axis::Z>(bind(&Node::SplitQuadtree, this, _1/*slpitted*/, _2/*begin*/, _3/*end*/, splitTechnique, splitPoint, _4/*overlapThreshold*/, _5/*idxOffset*/), splitted, begin, end, splitPoint, overlapThreshold);
	}

	template<class Object, class CustomNodeData, TreeStructure treeStructure>
	template<Axis axis, class F>
	void BVH<Object, CustomNodeData, treeStructure>::Node::Split3(const F &action, bool &splitted, typename std::enable_if_t<true, decltype(objects)>::iterator begin, typename std::enable_if_t<true, decltype(objects)>::iterator end, decltype(aabb.Center()) splitPoint, unsigned int idxOffset)
	{
		using namespace std;

		constexpr auto axisIdx = underlying_type_t<Axis>(axis);
		constexpr unsigned int idxOffsetStride = 1u | axisIdx << axisIdx;	// [1, 3, 9]

		const auto splitInternal = partition(begin, end, Object2AABB<AAABBInternalSplitter<axis>>(splitPoint[axisIdx]));
		splitted |= splitInternal != end;
		if (splitInternal != end)
		{
			const auto split = partition(splitInternal, end, Object2AABB<AAABBSplitter<axis>>(splitPoint[axisIdx]));
			if (split != splitInternal)
				action(splitted, splitInternal, split, idxOffset += idxOffsetStride);
			if (split != end)
				action(splitted, split, end, idxOffset += idxOffsetStride);
		}
		if (splitInternal != begin)
			action(splitted, begin, splitInternal, idxOffset);
	}

	template<class Object, class CustomNodeData, TreeStructure treeStructure>
	inline void BVH<Object, CustomNodeData, treeStructure>::Node::SplitEneaTree(bool &splitted, typename std::enable_if_t<true, decltype(objects)>::iterator begin, typename std::enable_if_t<true, decltype(objects)>::iterator end, SplitTechnique splitTechnique, decltype(aabb.Center()) splitPoint, unsigned int idxOffset)
	{
		using namespace std;
		using namespace placeholders;

		const auto createChildNode = bind(&Node::CreateChildNode<>, this, _1/*splitted*/, _2/*begin*/, _3/*end*/, splitTechnique, _4/*idxOffset*/);
		Split3<Axis::Y>(bind(&Node::Split3<Axis::X, decltype(cref(createChildNode))>, this, cref(createChildNode), _1/*splitted*/, _2/*begin*/, _3/*end*/, splitPoint, _4/*idxOffset*/), splitted, begin, end, splitPoint, idxOffset);
	}

	// 1 call site
	template<class Object, class CustomNodeData, TreeStructure treeStructure>
	inline void BVH<Object, CustomNodeData, treeStructure>::Node::SplitIcoseptree(bool &splitted, typename std::enable_if_t<true, decltype(objects)>::iterator begin, typename std::enable_if_t<true, decltype(objects)>::iterator end, SplitTechnique splitTechnique, decltype(aabb.Center()) splitPoint)
	{
		using namespace std;
		using namespace placeholders;

		Split3<Axis::Z>(bind(&Node::SplitEneaTree, this, _1/*splitted*/, _2/*begin*/, _3/*end*/, splitTechnique, splitPoint, _4/*idxOffset*/), splitted, begin, end, splitPoint);
	}

	template<class Object, class CustomNodeData, TreeStructure treeStructure>
	inline auto BVH<Object, CustomNodeData, treeStructure>::Node::GetVisibility(OcclusionCullDomain override) const noexcept -> Visibility
	{
		return Visibility(underlying_type_t<Visibility>(visibility) & underlying_type_t<OcclusionCullDomain>(override));
	}

	template<class Object, class CustomNodeData, TreeStructure treeStructure>
	inline void BVH<Object, CustomNodeData, treeStructure>::Node::OverrideOcclusionCullDomain(OcclusionCullDomain &overriden) const noexcept
	{
		// ChildrenOnly -> ForceComposite is senseless
		assert(visibility == Visibility::Culled || overriden != OcclusionCullDomain::ChildrenOnly || occlusionCullDomain != OcclusionCullDomain::ForceComposite);
		reinterpret_cast<underlying_type_t<OcclusionCullDomain> &>(overriden) |= underlying_type_t<OcclusionCullDomain>(occlusionCullDomain);	// strict aliasing rules violation?
	}

	template<class Object, class CustomNodeData, TreeStructure treeStructure>
	template<typename ...Args, typename F>
	void BVH<Object, CustomNodeData, treeStructure>::Node::Traverse(F &nodeHandler, Args ...args)
	{
		using namespace std;

		if (nodeHandler(*this, args...))
		{
			for_each_n(cbegin(childrenOrder), childrenCount, [&](remove_extent_t<decltype(childrenOrder)> idx)
			{
				children[idx]->Traverse(nodeHandler, args...);
			});
		}
	}

	template<class Object, class CustomNodeData, TreeStructure treeStructure>
	std::pair<unsigned long int, bool> BVH<Object, CustomNodeData, treeStructure>::Node::Shcedule(const FrustumCuller<std::enable_if_t<true, decltype(aabb.Center())>::dimension> &frustumCuller, const HLSL::float4x4 &frustumXform, const HLSL::float4x3 *depthSortXform,
		bool parentInsideFrustum, float parentOcclusionCulledProjLength, float parentOcclusion)
	{
		using namespace std;

		// cull if necessary
		if (!parentInsideFrustum)
		{
			switch (frustumCuller.Cull<false>(aabb))
			{
			case CullResult::OUTSIDE:
				visibility = Visibility::Culled;
				shceduleOcclusionQuery = false;	// accessed during Issue
				return { GetInclusiveTriCount(), false };
			case CullResult::INSIDE:
				parentInsideFrustum = true;
				break;
			}
		}

		unsigned long int childrenCulledTris = 0;
		bool childQueryCanceled = false;

		const auto traverseChildren = [&]
		{
			if (childrenCount)
			{
#if MULTITHREADED_TREE_TRAVERSE
				// consider using thread pool instead of async
				future<pair<unsigned long int, bool>> childrenResults[extent_v<decltype(children)>];
				// launch
				transform(next(cbegin(children)), next(cbegin(children), childrenCount), begin(childrenResults), [=, /*&nodeHandler, */&frustumCuller, &frustumXform](const remove_extent_t<decltype(children)> &child)
				{
					return async(&Node::Shcedule, child.get(), /*cref(nodeHandler), */cref(frustumCuller), cref(frustumXform), depthSortXform, parentInsideFrustum, parentOcclusionCulledProjLength, parentOcclusion);
				});

				// traverse first child in this thread
				const auto childResult = children[0]->Shcedule(/*nodeHandler, */frustumCuller, frustumXform, depthSortXform, parentInsideFrustum, parentOcclusionCulledProjLength, parentOcclusion);
				childrenCulledTris = childResult.first;
				childQueryCanceled = childResult.second;
#else
				for_each_n(cbegin(children), childrenCount, [&, depthSortXform, parentInsideFrustum, parentOcclusionCulledProjLength, parentOcclusion](const remove_extent_t<decltype(children)> &child)
				{
					const auto childResult = child->Shcedule(/*nodeHandler, */frustumCuller, frustumXform, depthSortXform, parentInsideFrustum, parentOcclusionCulledProjLength, parentOcclusion);
					childrenCulledTris += childResult.first;
					childQueryCanceled |= childResult.second;
				});
#endif

				// sort if necessary
				if (depthSortXform)
				{
					// xform AABBs to view space
					AABB<3> viewSpaceAABBs[extent_v<decltype(children)>];
					transform(cbegin(children), next(cbegin(children), childrenCount), viewSpaceAABBs, [depthSortXform](const remove_extent_t<decltype(children)> &child)
					{
						return TransformAABB(child->aabb, *depthSortXform);
					});

					// sort by near AABB z (needed for occlusion culling to work properly for nested objects)
					sort(begin(childrenOrder), next(begin(childrenOrder), childrenCount), [&viewSpaceAABBs](remove_extent_t<decltype(childrenOrder)> left, remove_extent_t<decltype(childrenOrder)> right) -> bool
					{
						return viewSpaceAABBs[left].min.z < viewSpaceAABBs[right].min.z;
					});
				}

#if MULTITHREADED_TREE_TRAVERSE
				for_each_n(begin(childrenResults), childrenCount - 1, [&childrenCulledTris, &childQueryCanceled](remove_extent_t<decltype(childrenResults)> &childResult)
				{
					const auto resolvedResult = childResult.get();
					childrenCulledTris += resolvedResult.first;
					childQueryCanceled |= resolvedResult.second;
				});
#endif
			}

			visibility = childrenCulledTris ? Visibility::Composite : Visibility::Atomic;
		};

		if (OcclusionCulling::EarlyOut(GetInclusiveTriCount()))
		{
			// parentInsideFrustum now relates to this node
			if (parentInsideFrustum)
			{
				visibility = Visibility::Atomic;
				shceduleOcclusionQuery = false;
			}
			else
				traverseChildren();
		}
		else
		{
			// pre
#if 1
			const auto clipSpaceAABB = MakeClipSpaceAABB(frustumXform, GetAABB());
#else
			const ClipSpaceAABB clipSpaceAABB(frustumXform, GetAABB());
#endif
			const AABB<3> NDCSpaceAABB(clipSpaceAABB);
			const HLSL::float2 aabbProjSize = NDCSpaceAABB.Size();
			const float aabbProjSquare = aabbProjSize.x * aabbProjSize.y;
			const float aabbProjLength = fmax(aabbProjSize.x, aabbProjSize.y);
			bool cancelQueryDueToParent = false;
			// TODO: replace 'z >= 0 && w > 0' with 'w >= znear' and use 2D NDC space AABB
			if (shceduleOcclusionQuery = NDCSpaceAABB.min.z >= 0.f && clipSpaceAABB.MinW() > 0.f && OcclusionCulling::QueryBenefit<false>(aabbProjSquare, GetInclusiveTriCount()) &&
				!(cancelQueryDueToParent = (parentOcclusionCulledProjLength <= OcclusionCulling::nodeProjLengthThreshold || aabbProjLength / parentOcclusionCulledProjLength >= OcclusionCulling::nestedNodeProjLengthShrinkThreshold) && parentOcclusion < OcclusionCulling::parentOcclusionThreshold))
			{
				parentOcclusionCulledProjLength = aabbProjLength;
				parentOcclusion = GetOcclusion();
			}
			else
				parentOcclusion += GetOcclusion() - parentOcclusion * GetOcclusion();

			traverseChildren();

			// post
			assert(!(shceduleOcclusionQuery && cancelQueryDueToParent));
			__assume(!(shceduleOcclusionQuery && cancelQueryDueToParent));
			/*
			shceduleOcclusionQuery == true (=> cancelQueryDueToParent == false)									|	reevaluate shceduleOcclusionQuery if childQueryCanceled == false, otherwise keep shceduled unconditionally
			cancelQueryDueToParent == true (=> shceduleOcclusionQuery == false) && childQueryCanceled == false	|	reevaluate cancelQueryDueToParent and propagate it as childQueryCanceled
			shceduleOcclusionQuery == false && childQueryCanceled == true										|	propagate childQueryCanceled == true unconditionally
			*/
			if (shceduleOcclusionQuery || cancelQueryDueToParent && !childQueryCanceled)
			{
				const unsigned long int restTris = GetInclusiveTriCount() - childrenCulledTris;
				bool queryNeeded = childQueryCanceled || OcclusionCulling::QueryBenefit<true>(aabbProjSquare, restTris);
				if (queryNeeded)
				{
					const Node *boxes[OcclusionCulling::maxOcclusionQueryBoxes];
					const unsigned long int exludedTris = CollectOcclusionQueryBoxes(begin(boxes), end(boxes)).first;
					// reevaluate query benefit after excluding cheap objects during box collection
					if (queryNeeded = childQueryCanceled || OcclusionCulling::QueryBenefit<true>(aabbProjSquare, restTris - exludedTris))
					{
						childQueryCanceled = cancelQueryDueToParent;	// propagate if 'cancelQueryDueToParent == true', reset to false otherwise (shceduleOcclusionQuery == true)
						if (shceduleOcclusionQuery)
						{
							childrenCulledTris = GetInclusiveTriCount();	// ' - exludedTris' ?
							const auto boxesEnd = remove(begin(boxes), end(boxes), nullptr);
							// ...
						}
					}
				}
				shceduleOcclusionQuery &= queryNeeded;
			}
		}

		return { childrenCulledTris, childQueryCanceled };
	}

	// returns <exluded tris, accumulated AABB measure>
	template<class Object, class CustomNodeData, TreeStructure treeStructure>
	std::pair<unsigned long int, float> BVH<Object, CustomNodeData, treeStructure>::Node::CollectOcclusionQueryBoxes(const Node **boxesBegin, const Node **boxesEnd, Visibility parentVisibilityOverride)
	{
		using namespace std;

		assert(parentVisibilityOverride != Visibility::Culled);

		// reset 'culled' bit which can potetially be set in previous frame and not updated yet during Shcedule() due to early out
		reinterpret_cast<underlying_type_t<Visibility> &>(visibility) &= 0b01;
		
		reinterpret_cast<underlying_type_t<Visibility> &>(visibility) |= underlying_type_t<Visibility>(parentVisibilityOverride);

		if (parentVisibilityOverride == Visibility::Atomic)
			shceduleOcclusionQuery = false;	// need to set here because it may not be set in Shcedule() due to early out

		const auto childrenFilter = [parentAtomic = visibility == Visibility::Atomic](const remove_extent_t<decltype(children)> &child)
		{
			return parentAtomic || child->visibility != Visibility::Culled && !child->shceduleOcclusionQuery;
		};
		const auto filteredChildrenCount = count_if(cbegin(children), next(cbegin(children), childrenCount), childrenFilter);
		const auto boxesCount = distance(boxesBegin, boxesEnd);
		const float thisNodeMeasure = aabb.Measure();
		
		if (filteredChildrenCount > 0 && boxesCount >= filteredChildrenCount && GetExclusiveTriCount() <= OcclusionCulling::exclusiveTriCountCullThreshold)
		{
			unsigned long int excludedTris = GetExclusiveTriCount();
			float accumulatedChildrenMeasure = 0.f;
			const auto collectFromChild = [&, childrenFilter, minBoxesPerNode = boxesCount / filteredChildrenCount, additionalBoxes = boxesCount % filteredChildrenCount, segmentBegin = boxesBegin]
			(const remove_extent_t<decltype(children)> &child) mutable
			{
				if (childrenFilter(child))
				{
					auto segmentEnd = next(segmentBegin, minBoxesPerNode);
					if (additionalBoxes)
					{
						advance(segmentEnd, 1);
						additionalBoxes--;
					}
					const auto collectResults = child->CollectOcclusionQueryBoxes(segmentBegin, segmentEnd, visibility);
					excludedTris += collectResults.first;
					accumulatedChildrenMeasure += collectResults.second;
					segmentBegin = segmentEnd;
				}
			};
			for_each_n(cbegin(children), childrenCount, collectFromChild);

			// return children boxes only if they are smaller than this node's box
			if (accumulatedChildrenMeasure / thisNodeMeasure < OcclusionCulling::accumulatedChildrenMeasureShrinkThreshold)
			{
				occlusionCullDomain = excludedTris ?
					excludedTris == GetExclusiveTriCount() ? OcclusionCullDomain::ChildrenOnly : OcclusionCullDomain::ForceComposite
					: OcclusionCullDomain::WholeNode;
				return { excludedTris, accumulatedChildrenMeasure };
			}
		}

		*boxesBegin = this;
		fill(next(boxesBegin), boxesEnd, nullptr);
		occlusionCullDomain = OcclusionCullDomain::WholeNode;
		return { 0ul, thisNodeMeasure };
	}

	template<class Object, class CustomNodeData, TreeStructure treeStructure>
	template<typename Iterator>
	BVH<Object, CustomNodeData, treeStructure>::BVH(Iterator objBegin, Iterator objEnd, SplitTechnique splitTechnique, ...) : objects(objBegin, objEnd)
	{
		switch (treeStructure)
		{
		case ENNEATREE:
		case ICOSEPTREE:
			root = std::make_unique<Node>(objects.begin(), objects.end(), splitTechnique);
			break;
		case QUADTREE:
		case OCTREE:
			va_list params;
			va_start(params, splitTechnique);
			const double overlapThreshold = va_arg(params, double);
			va_end(params);
			assert(isgreaterequal(overlapThreshold, 0.));
			assert(islessequal(overlapThreshold, 1.));
			root = std::make_unique<Node>(objects.begin(), objects.end(), splitTechnique, overlapThreshold);
			break;
		}
	}

	// nodeHandler returns false to stop traversal
	template<class Object, class CustomNodeData, TreeStructure treeStructure>
	template<typename ...Args, typename F>
	inline void BVH<Object, CustomNodeData, treeStructure>::Traverse(F &nodeHandler, const Args &...args)
	{
		root->Traverse(nodeHandler, args...);
	}

	template<class Object, class CustomNodeData, TreeStructure treeStructure>
	inline void BVH<Object, CustomNodeData, treeStructure>::Shcedule(const FrustumCuller<std::enable_if_t<true, decltype(std::declval<Object>().GetAABB().Center())>::dimension> &frustumCuller, const HLSL::float4x4 &frustumXform, const HLSL::float4x3 *depthSortXform)
	{
		root->Shcedule(frustumCuller, frustumXform, depthSortXform);
	}

	template<class Object, class CustomNodeData, TreeStructure treeStructure>
	void BVH<Object, CustomNodeData, treeStructure>::FreeObjects()
	{
		objects.clear();
		objects.shrink_to_fit();
	}
}

#undef MULTITHREADED_TREE_TRAVERSE