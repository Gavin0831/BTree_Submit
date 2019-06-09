//
// Created by 郑文鑫 on 2019-03-09.
//

#include "utility.hpp"
#include <functional>
#include <cstddef>
#include "exception.hpp"
#include <map>
#include <cstring>
#include <iostream>
namespace sjtu {
    int cnt=0;
    template <class Key, class Value, class Compare = std::less<Key> >
    class BTree {
    public:
        typedef pair<Key, Value> value_type;
        class iterator;
        class const_iterator;
    private:
        static const int M=1024;
        static const int L=512;

        struct internal_Node
        {
            ssize_t offset;
            ssize_t parent;
            Key key[M+1];
            ssize_t child[M+1];
            int num;
            bool childIsLeaf;
            internal_Node()
            {
                offset=0;
                parent=0;
                for (int i=0;i<M+1;++i)
                    child[i]=0;
                num=0;
                childIsLeaf=false;
            }
        };

        struct leaf_Node
        {
            ssize_t offset;
            ssize_t parent;
            ssize_t previous,next;
            int num;
            value_type data[L+1];
            leaf_Node()
            {
                offset=0;
                parent=0;
                previous=0;
                next=0;
                num=0;
            }
        };

        struct Info
        {
            size_t num;
            ssize_t head;
            ssize_t back;
            ssize_t root;
            ssize_t eof;
            Info()
            {
                num = 0;
                head = 0;
                back = 0;
                root = 0;
                eof = 0;
            }
        };
        static const int info_offset=0;

        struct filename
        {
            char *ch;
            filename(){ch=new char[14];}
            ~filename(){delete ch;}
            void initialize(int no)
            {
                strcpy(ch,"excited .data");
                ch[7]=no+'0';
            }
            void initialize(char *ch1)
            {
                strcpy(ch,ch1);
            }
        };

        Info info;
        FILE *fp;
        FILE *fpCopy;
        bool isOpen;
        filename fpName;
        filename fpCopyName;
        bool fileExists;

        inline void openFile()
        {
            fileExists=true;
            if (!isOpen)
            {
                fp=fopen(fpName.ch,"rb+");
                if (fp== nullptr)
                {
                    fileExists=false;
                    fp=fopen(fpName.ch,"w");
                    fclose(fp);
                    fp=fopen(fpName.ch,"rb+");
                }
                else readFile(&info,info_offset,1,sizeof(Info));
                isOpen=true;
            }
        }

        inline void closeFile()
        {
            if (isOpen)
            {
                fclose(fp);
                isOpen=false;
            }
        }

        inline void readFile(void *ptr,ssize_t offset,size_t num,size_t size) const
        {
            fseek(fp,offset,SEEK_SET);
            fread(ptr,size,num,fp);
        }

        inline void writeFile(void *ptr,ssize_t offset,size_t num,size_t size)
        {
            fseek(fp,offset,SEEK_SET);
            fwrite(ptr,size,num,fp);
        }

        ssize_t prev_offset;

        inline void copy_read(void *ptr,ssize_t offset,size_t num,size_t size)
        {
            fseek(fpCopy,offset,SEEK_SET);
            fread(ptr,num,size,fpCopy);
        }

        inline void copyLeaf(ssize_t offset,ssize_t copy_offset,ssize_t parent_offset)
        {
            leaf_Node leaf,copy,pre;
            copy_read(&copy,copy_offset,1, sizeof(leaf_Node));
            leaf.offset=offset;
            leaf.parent=parent_offset;
            leaf.num=copy.num;
            leaf.previous=prev_offset;
            leaf.next=0;
            if (prev_offset!=0)
            {
                readFile(&pre,prev_offset,1, sizeof(leaf_Node));
                pre.next=offset;
                writeFile(&pre,prev_offset,1, sizeof(leaf_Node));
                info.back=offset;
            }
            else
                info.head=offset;
            for (int i=0;i<leaf.num;++i)
            {
                leaf.data[i].first=copy.data[i].first;
                leaf.data[i].second=copy.data[i].second;
            }
            writeFile(&leaf,offset,1, sizeof(leaf_Node));
            info.eof+= sizeof(leaf_Node);
            prev_offset=offset;
        }

        inline void copyNode(ssize_t offset,ssize_t copy_offset,ssize_t parent_offset)
        {
            internal_Node node,copy;
            copy_read(&copy,copy_offset,1, sizeof(internal_Node));
            writeFile(&node,offset,1, sizeof(internal_Node));
            info.eof+= sizeof(internal_Node);
            node.offset=offset;
            node.parent=parent_offset;
            node.num=copy.num;
            node.childIsLeaf=copy.childIsLeaf;
            for (int i=0;i<node.num;+i)
            {
                node.key[i]=copy.key[i];
                if (node.childIsLeaf)
                    copyLeaf(info.eof,copy.child[i],offset);
                else
                    copyNode(info.eof,copy.child[i],offset);
            }
            writeFile(&node,offset,1, sizeof(internal_Node));
        }

        inline void copyFile(char *target,char *copy)
        {
            fpCopyName.initialize(copy);
            fpCopy=fopen(fpCopyName.ch,"rb+");
            Info copyinfo;
            copy_read(&copyinfo,info_offset,1, sizeof(Info));
            prev_offset=0;
            info.num=copyinfo.num;
            info.root=info.eof= sizeof(Info);
            writeFile(&info,info_offset,1,sizeof(Info));
            copyNode(info.root,copyinfo.root,0);
            writeFile(&info,info_offset,1, sizeof(Info));
            fclose(fpCopy);
        }

        inline void buildTree()
        {
            info.num=0;
            info.eof= sizeof(Info);
            internal_Node root;
            root.offset=info.eof;
            info.root=root.offset;
            info.eof+=sizeof(internal_Node);
            leaf_Node leaf;
            leaf.offset=info.eof;
            info.head=info.back=leaf.offset;
            info.eof+= sizeof(leaf_Node);
            root.parent=0;
            root.num=1;
            root.childIsLeaf=true;
            root.child[0]=leaf.offset;
            leaf.parent=root.offset;
            leaf.next=leaf.previous=0;
            leaf.num=0;
            writeFile(&info,info_offset,1,sizeof(Info));
            writeFile(&root,root.offset,1,sizeof(internal_Node));
            writeFile(&leaf,leaf.offset,1,sizeof(leaf_Node));
        }

        ssize_t findLeaf(const Key &key,ssize_t offset) const
        {
            internal_Node p;
            readFile(&p,offset,1, sizeof(internal_Node));
            if (p.childIsLeaf)
            {
                int pos=0;
                for (;pos<p.num;++pos)
                {
                    if (key<p.key[pos])
                        break;
                }
                if (pos==0)
                    return 0;
                return p.child[pos-1];
            }
            else
            {
                int pos=0;
                for (;pos<p.num;++pos)
                {
                    if (key<p.key[pos])
                        break;
                }
                if (pos==0)
                    return 0;
                return findLeaf(key,p.child[pos-1]);
            }
        }

        pair<iterator,OperationResult> leaf_insertion(leaf_Node &leaf,const Key &key,const Value &value)
        {
            iterator p;
            int pos=0;
            for (;pos<leaf.num;++pos)
            {
                if (key==leaf.data[pos].first)
                    return pair<iterator,OperationResult>(iterator(),Fail);
                if (key<leaf.data[pos].first)
                    break;
            }
            for (int i=leaf.num-1;i>=pos;--i)
            {
                leaf.data[i+1].first=leaf.data[i].first;
                leaf.data[i+1].second=leaf.data[i].second;
            }
            leaf.data[pos].first=key;
            leaf.data[pos].second=value;
            ++leaf.num;
            ++info.num;
            p.tree=this;
            p.pos=pos;
            p.offset=leaf.offset;
            writeFile(&info,info_offset,1,sizeof(Info));
            if (leaf.num<=L)
                writeFile(&leaf,leaf.offset,1, sizeof(leaf_Node));
            else {
                LeafDivision(leaf, p, key);
            }
            return pair<iterator,OperationResult>(p,Success);
        }

        void LeafDivision(leaf_Node &leaf,iterator &p,const Key &key)
        {
            leaf_Node l;
            l.num=leaf.num-leaf.num/2;
            leaf.num/=2;
            l.offset=info.eof;
            info.eof+= sizeof(leaf_Node);
            l.parent=leaf.parent;
            for (int i=0;i<l.num;++i)
            {
                l.data[i].first=leaf.data[i+leaf.num].first;
                l.data[i].second=leaf.data[i+leaf.num].second;
                if (l.data[i].first==key)
                {
                    p.offset=l.offset;
                    p.pos=i;
                }
            }
            l.next=leaf.next;
            l.previous=leaf.offset;
            leaf.next=l.offset;
            leaf_Node nextleaf;
            if (l.next==0)
                info.back=l.offset;
            else
            {
                readFile(&nextleaf,l.next,1, sizeof(leaf_Node));
                nextleaf.previous=l.offset;
                writeFile(&nextleaf,nextleaf.offset,1, sizeof(leaf_Node));
            }
            writeFile(&info,info_offset,1, sizeof(Info));
            writeFile(&leaf,leaf.offset,1, sizeof(leaf_Node));
            writeFile(&l,l.offset,1, sizeof(leaf_Node));
            internal_Node parent;
            readFile(&parent,leaf.parent,1, sizeof(Info));
            node_insertion(parent,l.data[0].first,l.offset);
        }

        void node_insertion(internal_Node &node,const Key &key,ssize_t child)
        {
            int pos=0;
            for (;pos<node.num;++pos) {
                if (key < node.key[pos])
                    break;
            }
            for (int i=node.num-1;i>=pos;--i) {
                node.key[i + 1] = node.key[i];
                node.child[i + 1] = node.child[i];
            }
            node.key[pos]=key;
            node.child[pos]=child;
            ++node.num;
            if (node.num<=M)
                writeFile(&node,node.offset,1, sizeof(internal_Node));
            else NodeDivision(node);
        }

        void NodeDivision(internal_Node &node) {
            internal_Node n;
            n.num = node.num - node.num / 2;
            node.num /= 2;
            n.parent = node.parent;
            n.childIsLeaf = node.childIsLeaf;
            n.offset = info.eof;
            info.eof += sizeof(internal_Node);
            for (int i = 0; i < n.num; ++i) {
                n.key[i] = node.key[i + node.num];
                n.child[i] = node.child[i + node.num];
            }
            leaf_Node leaf;
            internal_Node internode;
            for (int i = 0; i < n.num; ++i) {
                if (n.childIsLeaf) {
                    readFile(&leaf, n.child[i], 1, sizeof(leaf_Node));
                    leaf.parent = n.offset;
                    writeFile(&leaf, leaf.offset, 1, sizeof(leaf_Node));
                } else {
                    readFile(&internode, n.child[i], 1, sizeof(internal_Node));
                    internode.parent = n.offset;
                    writeFile(&internode, internode.offset, 1, sizeof(internal_Node));
                }
            }
            if (node.offset == info.root) {
                internal_Node root;
                root.parent = 0;
                root.childIsLeaf = false;
                root.offset = info.eof;
                info.eof += sizeof(internal_Node);
                root.num = 2;
                root.key[0] = node.key[0];
                root.child[0] = node.offset;
                root.key[1] = n.key[0];
                root.child[1] = n.offset;
                node.parent = root.offset;
                n.parent = root.offset;
                info.root = root.offset;
                writeFile(&info, info_offset, 1, sizeof(Info));
                writeFile(&node, node.offset, 1, sizeof(internal_Node));
                writeFile(&n, n.offset, 1, sizeof(internal_Node));
                writeFile(&root, root.offset, 1, sizeof(internal_Node));
            } else {
                writeFile(&info, info_offset, 1, sizeof(Info));
                writeFile(&node, node.offset, 1, sizeof(internal_Node));
                writeFile(&n, n.offset, 1, sizeof(internal_Node));
                internal_Node parent;
                readFile(&parent, node.parent, 1, sizeof(internal_Node));
                node_insertion(parent, n.key[0], n.offset);
            }
        }

    public:
        class iterator {
            friend class BTree;
        private:
            BTree *tree;
            ssize_t offset;
            int pos;
        public:
            bool modify(const Value& value){
                leaf_Node l;
                tree->readFile(&l,offset,1, sizeof(leaf_Node));
                l.data[pos].second=value;
                tree->writeFile(&l,offset,1, sizeof(leaf_Node));
                return true;
            }
            iterator() {
                // TODO Default Constructor
                tree= nullptr;
                pos=0;
                offset=0;
            }
            iterator(const iterator& other) {
                // TODO Copy Constructor
                tree=other.tree;
                offset=other.offset;
                pos=other.pos;
            }
            iterator(const const_iterator& other) {
                // TODO Copy Constructor
                tree=other.tree;
                offset=other.offset;
                pos=other.pos;
            }
            iterator(BTree *copytree, ssize_t copyoffset = 0,int copypos= 0) {
                tree =copytree;
                offset = copyoffset;
                pos = copypos;
            }
            Value getValue()
            {
                leaf_Node leaf;
                tree->readFile(&leaf,offset,1, sizeof(leaf_Node));
                return leaf.data[pos].second;
            }
            iterator operator++(int) {
                // Todo iterator++
                iterator p=*this;
                if (p==tree->end())
                {
                    tree= nullptr;
                    pos=0;
                    offset=0;
                    return p;
                }
                leaf_Node l;
                tree->readFile(&l,offset,1, sizeof(leaf_Node));
                if (pos==l.num-1)
                {
                    if (l.next==0)
                        ++pos;
                    else
                    {
                        offset=l.next;
                        pos=0;
                    }
                }
                else
                    ++pos;
                return p;
            }
            iterator& operator++() {
                // Todo ++iterator
                if (*this==tree->end())
                {
                    tree= nullptr;
                    pos=0;
                    offset=0;
                    return *this;
                }
                leaf_Node l;
                tree->readFile(&l,offset,1, sizeof(leaf_Node));
                if (pos==l.num-1)
                {
                    if (l.next==0)
                        ++pos;
                    else
                    {
                        offset=l.next;
                        pos=0;
                    }
                }
                else
                    ++pos;
                return *this;
            }
            iterator operator--(int) {
                // Todo iterator--
                iterator p=*this;
                if (*this==tree->begin())
                {
                    tree= nullptr;
                    pos=0;
                    offset=0;
                    return p;
                }
                leaf_Node l1,l2;
                tree->readFile(&l1,offset,1, sizeof(leaf_Node));
                if (pos==0)
                {
                    offset=l1.previous;
                    tree->readFile(&l2,l1.previous,1, sizeof(leaf_Node));
                    pos=l2.num-1;
                }
                else
                    --pos;
                return p;
            }
            iterator& operator--() {
                // Todo --iterator
                if (*this==tree->begin())
                {
                    tree= nullptr;
                    pos=0;
                    offset=0;
                    return *this;
                }
                leaf_Node l1,l2;
                tree->readFile(&l1,offset,1, sizeof(leaf_Node));
                if (pos==0)
                {
                    offset=l1.previous;
                    tree->readFile(&l2,l1.previous,1, sizeof(leaf_Node));
                    pos=l2.num-1;
                }
                else
                    --pos;
                return *this;
            }

            bool operator==(const iterator& rhs) const {
                // Todo operator ==
                return tree==rhs.tree && offset==rhs.offset && pos==rhs.pos;
            }
            bool operator==(const const_iterator& rhs) const {
                // Todo operator ==
                return tree==rhs.tree && offset==rhs.offset && pos==rhs.pos;
            }
            bool operator!=(const iterator& rhs) const {
                // Todo operator !=
                return tree!=rhs.tree || offset!=rhs.offset || pos!=rhs.pos;
            }
            bool operator!=(const const_iterator& rhs) const {
                // Todo operator !=
                return tree!=rhs.tree || offset!=rhs.offset || pos!=rhs.pos;
            }
        };
        class const_iterator {
            friend class BTree;
        private:
            const BTree *tree;
            ssize_t offset;
            int pos;
        public:
            const_iterator() {
                // TODO
                tree= nullptr;
                pos=0;
                offset=0;
            }
            const_iterator(const BTree *copytree,ssize_t copyoffset,int copypos=0)
            {
                tree=copytree;
                offset=copyoffset;
                pos=copypos;
            }
            const_iterator(const const_iterator& other) {
                // TODO
                tree=other.tree;
                offset=other.offset;
                pos=other.pos;
            }
            const_iterator(const iterator& other) {
                // TODO
                tree=other.tree;
                offset=other.offset;
                pos=other.pos;
            }

            const_iterator operator++(int)
            {
                const_iterator p=*this;
                if (*this==tree->cend())
                {
                    tree= nullptr;
                    pos=0;
                    offset=0;
                    return p;
                }
                leaf_Node l;
                tree->readFile(&p,offset,1, sizeof(leaf_Node));
                if (pos == l.num-1)
                {
                    if (l.next==0)
                        ++pos;
                    else
                    {
                        offset=l.next;
                        pos=0;
                    }
                }
                else
                    ++pos;
                return p;
            }
            const_iterator &operator++()
            {
                if (*this==tree->cend())
                {
                    tree= nullptr;
                    pos=0;
                    offset=0;
                    return *this;
                }
                leaf_Node l;
                tree->readFile(&l,offset,1, sizeof(leaf_Node));
                if (pos == l.num-1)
                {
                    if (l.next==0)
                        ++pos;
                    else
                    {
                        offset=l.next;
                        pos=0;
                    }
                }
                else
                    ++pos;
                return *this;
            }
            const_iterator operator--(int)
            {
                const_iterator p=*this;
                if (*this == tree->cbegin())
                {
                    tree= nullptr;
                    pos=0;
                    offset=0;
                    return p;
                }
                leaf_Node l1,l2;
                tree->readFile(&l1,offset,1, sizeof(leaf_Node));
                if (pos==0)
                {
                    offset=l1.previous;
                    tree->readFile(&l2,l1.previous,1, sizeof(leaf_Node));
                    pos=l2.num-1;
                }
                else
                    --pos;
                return p;
            }
            const_iterator &operator--()
            {
                if (*this == tree->cbegin())
                {
                    tree= nullptr;
                    pos=0;
                    offset=0;
                    return *this;
                }
                leaf_Node l1,l2;
                tree->readFile(&l1,offset,1, sizeof(leaf_Node));
                if (pos==0)
                {
                    offset=l1.previous;
                    tree->readFile(&l2,l1.previous,1, sizeof(leaf_Node));
                    pos=l2.num-1;
                }
                else
                    --pos;
                return *this;
            }
            bool operator==(const iterator& rhs) const {
                return tree==rhs.tree && offset==rhs.offset && pos==rhs.pos;
            }
            bool operator==(const const_iterator& rhs) const {
                return tree==rhs.tree && offset==rhs.offset && pos==rhs.pos;
            }
            bool operator!=(const iterator& rhs) const {
                return tree!=rhs.tree || offset!=rhs.offset || pos!=rhs.pos;
            }
            bool operator!=(const const_iterator& rhs) const {
                return tree!=rhs.tree || offset!=rhs.offset || pos!=rhs.pos;
            }
        };

        BTree() {
            // Todo Default
            fpName.initialize(cnt);
            ++cnt;
            fp= nullptr;
            openFile();
            if (!fileExists)
                buildTree();
        }
        BTree(const BTree& other) {
            // Todo Copy
            fpName.initialize(cnt);
            ++cnt;
            openFile();
            copyFile(fpName.ch,other.fpName.ch);
        }
        BTree& operator=(const BTree& other) {
            // Todo Assignment
            fpName.initialize(cnt);
            ++cnt;
            openFile();
            copyFile(fpName.ch,other.fpName.ch);
        }
        ~BTree() {
            // Todo Destructor
            closeFile();
        }

        pair<iterator, OperationResult> insert(const Key& key, const Value& value) {
            ssize_t leaf_offset=findLeaf(key,info.root);
            leaf_Node leaf;
            if (info.num==0 || leaf_offset ==0)
            {
                readFile(&leaf,info.head,1, sizeof(leaf_Node));
                pair<iterator,OperationResult> p=leaf_insertion(leaf,key,value);
                if (p.second==Fail)
                    return p;
                ssize_t offset=leaf.parent;
                internal_Node node;
                while (offset!=0)
                {
                    readFile(&node,offset,1, sizeof(internal_Node));
                    node.key[0]=key;
                    writeFile(&node,offset,1,sizeof(internal_Node));
                    offset=node.parent;
                }
                return p;
            }
            readFile(&leaf,leaf_offset,1, sizeof(leaf_Node));
            pair<iterator,OperationResult> p=leaf_insertion(leaf,key,value);
            return p;
        }

        OperationResult erase(const Key& key) {
            // TODO erase function
            return Fail;  // If you can't finish erase part, just remaining here.
        }

        iterator begin() {
            return iterator(this,info.head,0);
        }
        const_iterator cbegin() const {
            return const_iterator(this,info.head,0);
        }

        iterator end() {
            leaf_Node back;
            readFile(&back,info.back,1, sizeof(leaf_Node));
            return iterator(this,info.back,back.num);
        }
        const_iterator cend() const {
            leaf_Node back;
            readFile(&back,info.back,1, sizeof(leaf_Node));
            return const_iterator(this,info.back,back.num);
        }

        bool empty() const {
            return info.num==0;
        }

        size_t size() const {
            return info.num;
        }

        void clear() {
            fp=fopen(fpName.ch,"w");
            fclose(fp);
            openFile();
            buildTree();
        }

        Value at(const Key& key){
            iterator p=find(key);
            leaf_Node leaf;
            if (p==end())
                throw index_out_of_bound();
            readFile(&leaf,p.offset,1, sizeof(leaf_Node));
            return leaf.data[p.pos].second;
        }

        size_t count(const Key& key) const {
            return size_t(find(key)!=iterator(nullptr));
        }

        iterator find(const Key& key) {
            ssize_t leaf_offset=findLeaf(key,info.root);
            if (leaf_offset==0)
                return end();
            leaf_Node leaf;
            readFile(&leaf,leaf_offset,1, sizeof(leaf_Node));
            for (int i=0;i<leaf.num;++i) {
                if (leaf.data[i].first == key)
                    return iterator(this, leaf_offset, i);
            }
            return end();
        }
        const_iterator find(const Key& key) const {
            ssize_t leaf_offset=findLeaf(key,info.root);
            if (leaf_offset==0)
                return cend();
            leaf_Node leaf;
            readFile(&leaf,leaf_offset,1, sizeof(leaf_Node));
            for (int i=0;i<leaf.num;++i) {
                if (leaf.data[i].first == key)
                    return const_iterator(this, leaf_offset, i);
            }
            return cend();
        }
    };
}  // namespace sjtu

