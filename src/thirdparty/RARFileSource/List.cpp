/*
 * Copyright (C) 2008-2012, OctaneSnail <os@v12pwr.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

template <class T> void CRFSList<T>::InsertFirst (CRFSNode<T> *n)
{
	n->next = anchor.next;
	n->prev = &anchor;

	anchor.next->prev = n;
	anchor.next = n;
}

template <class T> void CRFSList<T>::InsertLast (CRFSNode<T> *n)
{
	n->next = &anchor;
	n->prev = anchor.prev;

	anchor.prev->next = n;
	anchor.prev = n;
}

template <class T> T *CRFSList<T>::UnlinkFirst (void)
{
	CRFSNode<T> *n = First ();
	if (!n)
		return NULL;
	n->Unlink ();
	return (T *) n;
}

template <class T> T *CRFSList<T>::UnlinkLast (void)
{
	CRFSNode<T> *n = Last ();
	if (!n)
		return NULL;
	n->Unlink ();
	return (T *) n;
}

template <class T> T *CRFSList<T>::Next (CRFSNode<T> *n)
{
	if (n->next == &anchor)
		return NULL;
	return (T *) n->next;
}

template <class T> T *CRFSList<T>::Prev (CRFSNode<T> *n)
{
	if (n->prev == &anchor)
		return NULL;
	return (T *) n->prev;
}

template <class T> void CRFSList<T>::Clear ()
{
	T *node = UnlinkFirst();

    while (node) {
        delete node;
        node = UnlinkFirst();
    }
}
