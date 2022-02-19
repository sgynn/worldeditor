#pragma once

#include <base/model.h>
#include <base/animationcontroller.h>
#include <base/xml.h>
#include <cstdio>

/// Model extension for storing animationbanks
class AnimationBankExtension : public base::ModelExtension {
	BASE_MODEL_EXTENSION;
	base::AnimationBank* animations;
	AnimationBankExtension(base::AnimationBank* a) : animations(a) {}
	~AnimationBankExtension() { delete animations; }
};

/// Model extension for layout heirachy
class LayoutExtension : public base::ModelExtension {
	BASE_MODEL_EXTENSION;
	struct Node {
		const char* mesh = 0;
		const char* name = 0;
		const char* bone = 0;
		vec3 position;
		Quaternion orientation;
		vec3 scale = vec3(1,1,1);
		size_t index = -1;
		Node* parent = 0;
		std::vector<Node*> children; 
	};

	public:
	static ModelExtension* construct(const base::XMLElement& e) { return new LayoutExtension(e); }

	LayoutExtension(const base::XMLElement& layout) : rootNode(0) {
		if(layout.size()) {
			rootNode = new Node;
			parseLayout(rootNode, layout);
		}
	}

	~LayoutExtension() {
		for(Node* n: allNodes) {
			free((char*)n->mesh);
			free((char*)n->name);
			free((char*)n->bone);
			delete n;
		}
		delete rootNode;
	}

	const Node* findNode(const char* name) const {
		if(!name) return 0;
		for(Node* node: allNodes) if(node->name && strcmp(name, node->name)==0) return node;
		return 0;
	}

	static void getDerivedTransform(const Node* node, vec3& pos, Quaternion& rot) {
		pos = node->position;
		rot = node->orientation;
		if(!node->parent->parent) return;
		for(LayoutExtension::Node* p = node->parent; p->parent->parent; p=p->parent) {
			pos = p->position + p->orientation * pos;
			rot = p->orientation * rot;
		}
	}

	std::vector<Node*>::const_iterator begin() { return allNodes.begin(); }
	std::vector<Node*>::const_iterator end() { return allNodes.end(); }
	const Node* root() const { return rootNode; }
	
	protected:
	Node* rootNode;					// Root node of heirachy. Will be null if layout is empty
	std::vector<Node*> allNodes;	// Flat list of all nodes. Child nodes will never be before their parent


	private:
	void parseLayout(Node* parent, const base::XMLElement& e) {
		for(const base::XMLElement& i : e) {
			Node* n = new Node();
			const char* mesh = i.attribute("mesh");
			const char* name = i.attribute("name");
			const char* bone = i.attribute("bone");
			n->mesh = mesh[0]? strdup(mesh): 0;
			n->name = name[0]? strdup(name): 0;
			n->bone = bone[0]? strdup(bone): 0;
			n->index = allNodes.size();
			n->parent = parent;
			n->scale.set(1,1,1);
			sscanf(i.attribute("position"), "%f %f %f", &n->position.x, &n->position.y, &n->position.z);
			sscanf(i.attribute("orientation"), "%f %f %f %f", &n->orientation.w, &n->orientation.x, &n->orientation.y, &n->orientation.z);
			sscanf(i.attribute("scale"), "%f %f %f", &n->scale.x, &n->scale.y, &n->scale.z);
			parent->children.push_back(n);
			allNodes.push_back(n);
			parseLayout(n, i);
		}
	}
};

