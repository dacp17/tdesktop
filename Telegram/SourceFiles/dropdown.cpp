/*
This file is part of Telegram Desktop,
the official desktop version of Telegram messaging app, see https://telegram.org

Telegram Desktop is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

It is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU General Public License for more details.

Full license: https://github.com/telegramdesktop/tdesktop/blob/master/LICENSE
Copyright (c) 2014 John Preston, https://desktop.telegram.org
*/
#include "stdafx.h"

#include "dropdown.h"
#include "historywidget.h"

#include "localstorage.h"
#include "lang.h"

#include "window.h"
#include "apiwrap.h"

Dropdown::Dropdown(QWidget *parent, const style::dropdown &st) : TWidget(parent),
_ignore(false), _selected(-1), _st(st), _width(_st.width), _hiding(false), a_opacity(0), _shadow(_st.shadow) {
	resetButtons();

	_hideTimer.setSingleShot(true);
	connect(&_hideTimer, SIGNAL(timeout()), this, SLOT(hideStart()));

	if (cPlatform() == dbipMac) {
		connect(App::wnd()->windowHandle(), SIGNAL(activeChanged()), this, SLOT(onWndActiveChanged()));
	}
}

void Dropdown::ignoreShow(bool ignore) {
	_ignore = ignore;
}

void Dropdown::onWndActiveChanged() {
	if (!App::wnd()->windowHandle()->isActive() && !isHidden()) {
		leaveEvent(0);
	}
}

IconedButton *Dropdown::addButton(IconedButton *button) {
	button->setParent(this);

	int32 nw = _st.padding.left() + _st.padding.right() + button->width();
	if (nw > _width) {
		_width = nw;
		for (int32 i = 0, l = _buttons.size(); i < l; ++i) _buttons[i]->resize(_width - _st.padding.left() - _st.padding.right(), _buttons[i]->height());
	} else {
		button->resize(_width - _st.padding.left() - _st.padding.right(), button->height());
	}
	if (!button->isHidden()) {
		if (_height > _st.padding.top() + _st.padding.bottom()) {
			_height += _st.border;
		}
		_height += button->height();
	}
	_buttons.push_back(button);
	connect(button, SIGNAL(stateChanged(int, ButtonStateChangeSource)), this, SLOT(buttonStateChanged(int, ButtonStateChangeSource)));

	resize(_width, _height);

	return button;
}

void Dropdown::resetButtons() {
	_width = qMax(_st.padding.left() + _st.padding.right(), int(_st.width));
	_height = _st.padding.top() + _st.padding.bottom();
	for (int32 i = 0, l = _buttons.size(); i < l; ++i) {
		delete _buttons[i];
	}
	_buttons.clear();
	resize(_width, _height);

	_selected = -1;
}

void Dropdown::updateButtons() {
	int32 top = _st.padding.top(), starttop = top;
	for (Buttons::const_iterator i = _buttons.cbegin(), e = _buttons.cend(); i != e; ++i) {
		if (!(*i)->isHidden()) {
			(*i)->move(_st.padding.left(), top);
			if ((*i)->width() != _width - _st.padding.left() - _st.padding.right()) {
				(*i)->resize(_width - _st.padding.left() - _st.padding.right(), (*i)->height());
			}
			top += (*i)->height() + _st.border;
		}
	}
	_height = top + _st.padding.bottom() - (top > starttop ? _st.border : 0);
	resize(_width, _height);
}

void Dropdown::resizeEvent(QResizeEvent *e) {
	int32 top = _st.padding.top();
	for (Buttons::const_iterator i = _buttons.cbegin(), e = _buttons.cend(); i != e; ++i) {
		if (!(*i)->isHidden()) {
			(*i)->move(_st.padding.left(), top);
			top += (*i)->height() + _st.border;
		}
	}
}

void Dropdown::paintEvent(QPaintEvent *e) {
	QPainter p(this);

	if (animating()) {
		p.setOpacity(a_opacity.current());
	}

	// draw shadow
	QRect r(_st.padding.left(), _st.padding.top(), _width - _st.padding.left() - _st.padding.right(), _height - _st.padding.top() - _st.padding.bottom());
	_shadow.paint(p, r);

	if (!_buttons.isEmpty() && _st.border > 0) { // paint separators
		p.setPen(_st.borderColor->p);
		int32 top = _st.padding.top(), i = 0, l = _buttons.size();
		for (; i < l; ++i) {
			if (!_buttons.at(i)->isHidden()) break;
		}
		if (i < l) {
			top += _buttons.at(i)->height();
			for (++i; i < l; ++i) {
				if (!_buttons.at(i)->isHidden()) {
					p.fillRect(_st.padding.left(), top, _width - _st.padding.left() - _st.padding.right(), _st.border, _st.borderColor->b);
					top += _st.border + _buttons.at(i)->height();
				}
			}
		}
	}
}

void Dropdown::enterEvent(QEvent *e) {
	_hideTimer.stop();
	if (_hiding) showStart();
	return TWidget::enterEvent(e);
}

void Dropdown::leaveEvent(QEvent *e) {
	if (animating()) {
		hideStart();
	} else {
		_hideTimer.start(300);
	}
	return TWidget::leaveEvent(e);
}

void Dropdown::keyPressEvent(QKeyEvent *e) {
	if (e->key() == Qt::Key_Enter || e->key() == Qt::Key_Return) {
		if (_selected >= 0 && _selected < _buttons.size()) {
			emit _buttons[_selected]->clicked();
			return;
		}
	} else if (e->key() == Qt::Key_Escape) {
		hideStart();
		return;
	}
	if ((e->key() != Qt::Key_Up && e->key() != Qt::Key_Down) || _buttons.size() < 1) return;

	bool none = (_selected < 0 || _selected >= _buttons.size());
	int32 delta = (e->key() == Qt::Key_Down ? 1 : -1);
	int32 newSelected = none ? (e->key() == Qt::Key_Down ? 0 : _buttons.size() - 1) : (_selected + delta);
	if (newSelected < 0) {
		newSelected = _buttons.size() - 1;
	} else if (newSelected >= _buttons.size()) {
		newSelected = 0;
	}
	int32 startFrom = newSelected;
	while (_buttons.at(newSelected)->isHidden()) {
		newSelected += delta;
		if (newSelected < 0) {
			newSelected = _buttons.size() - 1;
		} else if (newSelected >= _buttons.size()) {
			newSelected = 0;
		}
		if (newSelected == startFrom) return;
	}
	if (!none) {
		_buttons[_selected]->setOver(false);
	}
	_selected = newSelected;
	_buttons[_selected]->setOver(true);
}

void Dropdown::buttonStateChanged(int oldState, ButtonStateChangeSource source) {
	if (source == ButtonByUser) {
		for (int32 i = 0, l = _buttons.size(); i < l; ++i) {
			if (_buttons[i]->getState() & Button::StateOver) {
				if (i != _selected) {
					_buttons[i]->setOver(false);
				}
			}
		}
	} else if (source == ButtonByHover) {
		bool found = false;
		for (int32 i = 0, l = _buttons.size(); i < l; ++i) {
			if (_buttons[i]->getState() & Button::StateOver) {
				found = true;
				if (i != _selected) {
					int32 sel = _selected;
					_selected = i;
					if (sel >= 0 && sel < _buttons.size()) {
						_buttons[sel]->setOver(false);
					}
				}
			}
		}
		if (!found) {
			_selected = -1;
		}
	}
}

void Dropdown::otherEnter() {
	_hideTimer.stop();
	showStart();
}

void Dropdown::otherLeave() {
	if (animating()) {
		hideStart();
	} else {
		_hideTimer.start(0);
	}
}

void Dropdown::fastHide() {
	if (animating()) {
		anim::stop(this);
	}
	a_opacity = anim::fvalue(0, 0);
	_hideTimer.stop();
	hide();
}

void Dropdown::adjustButtons() {
	for (Buttons::const_iterator i = _buttons.cbegin(), e = _buttons.cend(); i != e; ++i) {
		(*i)->setOpacity(a_opacity.current());
	}
}

void Dropdown::hideStart() {
	_hiding = true;
	a_opacity.start(0);
	anim::start(this);
}

void Dropdown::hideFinish() {
	emit hiding();
	hide();
	for (Buttons::const_iterator i = _buttons.cbegin(), e = _buttons.cend(); i != e; ++i) {
		(*i)->clearState();
	}
	_selected = -1;
}

void Dropdown::showStart() {
	if (!isHidden() && a_opacity.current() == 1) {
		return;
	}
	_selected = -1;
	_hiding = false;
	show();
	a_opacity.start(1);
	anim::start(this);
}

bool Dropdown::animStep(float64 ms) {
	float64 dt = ms / _st.duration;
	bool res = true;
	if (dt >= 1) {
		a_opacity.finish();
		if (_hiding) {
			hideFinish();
		}
		res = false;
	} else {
		a_opacity.update(dt, anim::linear);
	}
	adjustButtons();
	update();
	return res;
}

bool Dropdown::eventFilter(QObject *obj, QEvent *e) {
	if (e->type() == QEvent::Enter) {
		otherEnter();
	} else if (e->type() == QEvent::Leave) {
		otherLeave();
	} else if (e->type() == QEvent::MouseButtonPress && static_cast<QMouseEvent*>(e)->button() == Qt::LeftButton) {
		if (isHidden() || _hiding) {
			otherEnter();
		} else {
			otherLeave();
		}
	}
	return false;
}

DragArea::DragArea(QWidget *parent) : TWidget(parent),
	_hiding(false), _in(false), a_opacity(0), a_color(st::dragColor->c), _shadow(st::boxShadow) {
	setMouseTracking(true);
	setAcceptDrops(true);
}

void DragArea::mouseMoveEvent(QMouseEvent *e) {
	if (_hiding) return;

	bool newIn = QRect(st::dragPadding.left(), st::dragPadding.top(), width() - st::dragPadding.left() - st::dragPadding.right(), height() - st::dragPadding.top() - st::dragPadding.bottom()).contains(e->pos());
	if (newIn != _in) {
		_in = newIn;
		a_opacity.start(1);
		a_color.start((_in ? st::dragDropColor : st::dragColor)->c);
		anim::start(this);
	}
}

void DragArea::dragMoveEvent(QDragMoveEvent *e) {
	QRect r(st::dragPadding.left(), st::dragPadding.top(), width() - st::dragPadding.left() - st::dragPadding.right(), height() - st::dragPadding.top() - st::dragPadding.bottom());
	bool newIn = r.contains(e->pos());
	if (newIn != _in) {
		_in = newIn;
		a_opacity.start(1);
		a_color.start((_in ? st::dragDropColor : st::dragColor)->c);
		anim::start(this);
	}
	e->setDropAction(_in ? Qt::CopyAction : Qt::IgnoreAction);
	e->accept();
}

void DragArea::setText(const QString &text, const QString &subtext) {
	_text = text;
	_subtext = subtext;
	update();
}

void DragArea::paintEvent(QPaintEvent *e) {
	QPainter p(this);

	if (animating()) {
		p.setOpacity(a_opacity.current());
	}

	QRect r(st::dragPadding.left(), st::dragPadding.top(), width() - st::dragPadding.left() - st::dragPadding.right(), height() - st::dragPadding.top() - st::dragPadding.bottom());

	// draw shadow
	_shadow.paint(p, r);

	p.fillRect(r, st::white->b);

	p.setPen(a_color.current());

	p.setFont(st::dragFont->f);
	p.drawText(QRect(0, (height() - st::dragHeight) / 2, width(), st::dragFont->height), _text, QTextOption(style::al_top));

	p.setFont(st::dragSubfont->f);
	p.drawText(QRect(0, (height() + st::dragHeight) / 2 - st::dragSubfont->height, width(), st::dragSubfont->height * 2), _subtext, QTextOption(style::al_top));
}

void DragArea::dragEnterEvent(QDragEnterEvent *e) {
	static_cast<HistoryWidget*>(parentWidget())->dragEnterEvent(e);
	e->setDropAction(Qt::IgnoreAction);
	e->accept();
}

void DragArea::dragLeaveEvent(QDragLeaveEvent *e) {
	static_cast<HistoryWidget*>(parentWidget())->dragLeaveEvent(e);
	_in = false;
	a_opacity.start(_hiding ? 0 : 1);
	a_color.start((_in ? st::dragDropColor : st::dragColor)->c);
	anim::start(this);
}

void DragArea::dropEvent(QDropEvent *e) {
	static_cast<HistoryWidget*>(parentWidget())->dropEvent(e);
	if (e->isAccepted()) {
		emit dropped(e);
	}
}

void DragArea::otherEnter() {
	showStart();
}

void DragArea::otherLeave() {
	hideStart();
}

void DragArea::fastHide() {
	if (animating()) {
		anim::stop(this);
	}
	a_opacity = anim::fvalue(0, 0);
	hide();
}

void DragArea::hideStart() {
	_hiding = true;
	_in = false;
	a_opacity.start(0);
	a_color.start((_in ? st::dragDropColor : st::dragColor)->c);
	anim::start(this);
}

void DragArea::hideFinish() {
	hide();
	_in = false;
	a_color = anim::cvalue(st::dragColor->c);
}

void DragArea::showStart() {
	_hiding = false;
	show();
	a_opacity.start(1);
	a_color.start((_in ? st::dragDropColor : st::dragColor)->c);
	anim::start(this);
}

bool DragArea::animStep(float64 ms) {
	float64 dt = ms / st::dropdownDef.duration;
	bool res = true;
	if (dt >= 1) {
		a_opacity.finish();
		a_color.finish();
		if (_hiding) {
			hideFinish();
		}
		res = false;
	} else {
		a_opacity.update(dt, anim::linear);
		a_color.update(dt, anim::linear);
	}
	update();
	return res;
}

EmojiPanInner::EmojiPanInner(QWidget *parent) : QWidget(parent), _tab(cEmojiTab()), _selected(-1), _xSelected(-1), _pressedSel(-1), _xPressedSel(-1) {
	resize(EmojiPadPerRow * st::emojiPanSize.width(), EmojiPadRowsPerPage * st::emojiPanSize.height() - st::emojiPanSub);
	setMouseTracking(true);
	setFocusPolicy(Qt::NoFocus);

	_saveConfigTimer.setSingleShot(true);
	connect(&_saveConfigTimer, SIGNAL(timeout()), this, SLOT(onSaveConfig()));
}

void EmojiPanInner::paintEvent(QPaintEvent *e) {
	QPainter p(this);

	QRect r = e ? e->rect() : rect();

	if (_tab == dbietStickers) {
		int32 size = _stickers.size();
		float64 stickerWidth = width() / float64(StickerPadPerRow);
		int32 rows = (size / StickerPadPerRow) + ((size % StickerPadPerRow) ? 1 : 0), stickerSize = int32(stickerWidth);
		int32 fromrow = qMax(qFloor(r.top() / stickerSize), 0), torow = qMin(qCeil(r.bottom() / stickerSize) + 1, rows);
		for (int32 i = fromrow; i < torow; ++i) {
			for (int32 j = 0; j < StickerPadPerRow; ++j) {
				int32 index = i * StickerPadPerRow + j;
				if (index >= size) break;

				float64 hover = _hovers[index];

				QPoint pos(qRound(j * stickerWidth), i * stickerSize);
				if (hover > 0) {
					p.setOpacity(hover);
					p.setBrush(st::emojiPanHover->b);
					p.setPen(Qt::NoPen);
					p.drawRoundedRect(QRect(pos, QSize(stickerSize, stickerSize)), st::stickerPanRound, st::stickerPanRound);
					p.setOpacity(1);
				}

				DocumentData *sticker = _stickers[index];
				bool already = !sticker->already().isEmpty(), hasdata = !sticker->data.isEmpty();
				if (!sticker->loader && sticker->status != FileFailed && !already && !hasdata) {
					sticker->save(QString());
				}
				if (sticker->sticker->isNull() && (already || hasdata)) {
					if (already) {
						sticker->sticker = ImagePtr(sticker->already());
					} else {
						sticker->sticker = ImagePtr(sticker->data);
					}
				}

				float64 coef = qMin((stickerWidth - st::stickerPanPadding * 2) / float64(sticker->dimensions.width()), (stickerSize - st::stickerPanPadding * 2) / float64(sticker->dimensions.height()));
				if (coef > 1) coef = 1;
				int32 w = qRound(coef * sticker->dimensions.width()), h = qRound(coef * sticker->dimensions.height());
				if (w < 1) w = 1;
				if (h < 1) h = 1;
				QPoint ppos = pos + QPoint((stickerSize - w) / 2, (stickerSize - h) / 2);
				if (sticker->sticker->isNull()) {
					p.drawPixmap(ppos, sticker->thumb->pix(w, h));
				} else {
					p.drawPixmap(ppos, sticker->sticker->pix(w, h));
				}

				if (hover > 0 && _isUserGen[index]) {
					float64 xHover = _hovers[_stickers.size() + index];

					QPoint xPos = pos + QPoint(stickerWidth - st::stickerPanDelete.pxWidth(), 0);
					p.setOpacity(hover * (xHover + (1 - xHover) * st::stickerPanDeleteOpacity));
					p.drawPixmap(xPos, App::sprite(), st::stickerPanDelete);
					p.setOpacity(1);
				}
			}
		}
	} else {
		int32 size = _emojis.size();
		int32 rows = (size / EmojiPadPerRow) + ((size % EmojiPadPerRow) ? 1 : 0);
		int32 fromrow = qMax(qFloor(r.top() / st::emojiPanSize.height()), 0), torow = qMin(qCeil(r.bottom() / st::emojiPanSize.height()) + 1, rows);
		for (int32 i = fromrow; i < torow; ++i) {
			for (int32 j = 0; j < EmojiPadPerRow; ++j) {
				int32 index = i * EmojiPadPerRow + j;
				if (index >= size) break;

				float64 hover = _hovers[index];

				QPoint w(j * st::emojiPanSize.width(), i * st::emojiPanSize.height());
				if (hover > 0) {
					p.setOpacity(hover);
					p.setBrush(st::emojiPanHover->b);
					p.setPen(Qt::NoPen);
					p.drawRoundedRect(QRect(w, st::emojiPanSize), st::emojiPanRound, st::emojiPanRound);
					p.setOpacity(1);
				}
				QRect r(_emojis[index]->x, _emojis[index]->y, st::emojiImgSize, st::emojiImgSize);
				p.drawPixmap(w + QPoint((st::emojiPanSize.width() - st::emojiSize) / 2, (st::emojiPanSize.height() - st::emojiSize) / 2), App::emojis(), r);
			}
		}
	}
}

void EmojiPanInner::mousePressEvent(QMouseEvent *e) {
	_lastMousePos = e->globalPos();
	updateSelected();
	_pressedSel = _selected;
	_xPressedSel = _xSelected;
}

void EmojiPanInner::mouseReleaseEvent(QMouseEvent *e) {
	_lastMousePos = e->globalPos();
	updateSelected();
	if (_xSelected == _xPressedSel && _xSelected >= 0 && _tab == dbietStickers) {
		RecentStickerPack recent(cRecentStickers());
		DocumentData *sticker = _stickers.at(_xSelected - _stickers.size());
		for (int32 i = 0, l = recent.size(); i < l; ++i) {
			if (recent.at(i).first == sticker) {
				recent.removeAt(i);
				cSetRecentStickers(recent);
				Local::writeRecentStickers();
				showEmojiPack(dbietStickers);
				updateSelected();
				break;
			}
		}
	} else if (_selected == _pressedSel && _selected >= 0) {
		if (_tab == dbietStickers) {
			if (_selected < _stickers.size()) {
				emit stickerSelected(_stickers[_selected]);
			}
		} else if (_selected < _emojis.size()) {
			EmojiPtr emoji(_emojis[_selected]);
			RecentEmojiPack recent(cGetRecentEmojis());
			RecentEmojiPack::iterator i = recent.begin(), e = recent.end();
			for (; i != e; ++i) {
				if (i->first == emoji) {
					++i->second;
					if (i->second > 0x8000) {
						for (RecentEmojiPack::iterator j = recent.begin(); j != e; ++j) {
							if (j->second > 1) {
								j->second /= 2;
							} else {
								j->second = 1;
							}
						}
					}
					for (; i != recent.begin(); --i) {
						if ((i - 1)->second > i->second) {
							break;
						}
						qSwap(*i, *(i - 1));
					}
					break;
				}
			}
			if (i == e) {
				while (recent.size() >= EmojiPadPerRow * EmojiPadRowsPerPage) recent.pop_back();
				recent.push_back(qMakePair(emoji, 1));
				for (i = recent.end() - 1; i != recent.begin(); --i) {
					if ((i - 1)->second > i->second) {
						break;
					}
					qSwap(*i, *(i - 1));
				}
			}
			cSetRecentEmojis(recent);
			_saveConfigTimer.start(SaveRecentEmojisTimeout);

			emit emojiSelected(emoji);
		}
	}
}

void EmojiPanInner::onSaveConfig() {
	Local::writeUserSettings();
}

void EmojiPanInner::mouseMoveEvent(QMouseEvent *e) {
	_lastMousePos = e->globalPos();
	updateSelected();
}

void EmojiPanInner::leaveEvent(QEvent *e) {
	clearSelection();
}

void EmojiPanInner::clearSelection(bool fast) {
	_lastMousePos = mapToGlobal(QPoint(-10, -10));
	if (fast) {
		if (_tab == dbietStickers) {
			_hovers = QVector<float64>(_stickers.size() * 2, 0);
		} else {
			_hovers = QVector<float64>(_emojis.size(), 0);
		}
		_emojiAnimations.clear();
		_selected = _pressedSel = _xSelected = _xPressedSel = -1;
		anim::stop(this);
	} else {
		updateSelected();
	}
}

void EmojiPanInner::updateSelected() {
	int32 selIndex = -1, xSelIndex = -1;
	QPoint p(mapFromGlobal(_lastMousePos));
	if (_tab == dbietStickers) {
		float64 stickerWidth = width() / float64(StickerPadPerRow);
		int32 stickerSize = int32(stickerWidth);
		if (p.x() >= 0 && p.y() >= 0 && p.x() < StickerPadPerRow * stickerWidth) {
			selIndex = qFloor(p.y() / stickerSize) * StickerPadPerRow + qFloor(p.x() / stickerWidth);
			if (selIndex >= _stickers.size()) {
				selIndex = -1;
			} else {
				int32 inx = p.x() - (selIndex % StickerPadPerRow) * stickerWidth, iny = p.y() - ((selIndex / StickerPadPerRow) * stickerSize);
				if (inx >= stickerWidth - st::stickerPanDelete.pxWidth() && iny < st::stickerPanDelete.pxHeight()) {
					xSelIndex = _stickers.size() + selIndex;
				}
			}
		}
	} else if (p.x() >= 0 && p.y() >= 0 && p.x() < EmojiPadPerRow * st::emojiPanSize.width()) {
		selIndex = qFloor(p.y() / st::emojiPanSize.height()) * EmojiPadPerRow + qFloor(p.x() / st::emojiPanSize.width());
		if (selIndex >= _emojis.size()) {
			selIndex = -1;
		}
	}
	bool startanim = false;
	if (selIndex != _selected) {
		if (_selected >= 0) {
			_emojiAnimations.remove(_selected + 1);
			if (_emojiAnimations.find(-_selected - 1) == _emojiAnimations.end()) {
				if (_emojiAnimations.isEmpty()) startanim = true;
				_emojiAnimations.insert(-_selected - 1, getms());
			}
		}
		_selected = selIndex;
		if (_selected >= 0) {
			_emojiAnimations.remove(-_selected - 1);
			if (_emojiAnimations.find(_selected + 1) == _emojiAnimations.end()) {
				if (_emojiAnimations.isEmpty()) startanim = true;
				_emojiAnimations.insert(_selected + 1, getms());
			}
		}
		setCursor((_selected >= 0) ? style::cur_pointer : style::cur_default);
	}
	if (xSelIndex != _xSelected) {
		if (_xSelected >= 0) {
			_emojiAnimations.remove(_xSelected + 1);
			if (_emojiAnimations.find(-_xSelected - 1) == _emojiAnimations.end()) {
				if (_emojiAnimations.isEmpty()) startanim = true;
				_emojiAnimations.insert(-_xSelected - 1, getms());
			}
		}
		_xSelected = xSelIndex;
		if (_xSelected >= 0) {
			_emojiAnimations.remove(-_xSelected - 1);
			if (_emojiAnimations.find(_xSelected + 1) == _emojiAnimations.end()) {
				if (_emojiAnimations.isEmpty()) startanim = true;
				_emojiAnimations.insert(_xSelected + 1, getms());
			}
		}
	}
	if (startanim) anim::start(this);
}

bool EmojiPanInner::animStep(float64 ms) {
	uint64 now = getms();
	for (EmojiAnimations::iterator i = _emojiAnimations.begin(); i != _emojiAnimations.end();) {
		float64 dt = float64(now - i.value()) / st::emojiPanDuration;
		if (dt >= 1) {
			_hovers[qAbs(i.key()) - 1] = (i.key() > 0) ? 1 : 0;
			i = _emojiAnimations.erase(i);
		} else {
			_hovers[qAbs(i.key()) - 1] = (i.key() > 0) ? dt : (1 - dt);
			++i;
		}
	}
	update();
	return !_emojiAnimations.isEmpty();
}

void EmojiPanInner::showEmojiPack(DBIEmojiTab packIndex) {
	_tab = packIndex;
	int32 h, size;
	if (packIndex == dbietStickers) {
		_emojis.clear();

		float64 stickerWidth = width() / float64(StickerPadPerRow);
		int32 stickerSize = int32(stickerWidth);

		int32 l = cRecentStickers().size();
		_stickers.resize(l);
		_isUserGen.resize(l);
		for (int32 i = 0; i < l; ++i) {
			DocumentData *sticker = _stickers[i] = cRecentStickers().at(i).first;
			_isUserGen[i] = (cRecentStickers().at(i).second < 0);
			if (i < StickerPadPerRow * ((EmojiPadRowsPerPage * st::emojiPanSize.height() - int(st::emojiPanSub)) / stickerSize + 1)) {
				bool already = !sticker->already().isEmpty(), hasdata = !sticker->data.isEmpty();
				if (!sticker->loader && sticker->status != FileFailed && !already && !hasdata) {
					sticker->save(QString());
				}
			}
		}

		size = _stickers.size();
		h = ((size / StickerPadPerRow) + ((size % StickerPadPerRow) ? 1 : 0)) * stickerSize;
		_hovers = QVector<float64>(size * 2, 0);
	} else {
		_emojis = emojiPack(packIndex);
		_stickers.clear();
		_isUserGen.clear();

		size = _emojis.size();
		h = ((size / EmojiPadPerRow) + ((size % EmojiPadPerRow) ? 1 : 0)) * st::emojiPanSize.height();
		_hovers = QVector<float64>(size, 0);
	}
	h = qMax(h, EmojiPadRowsPerPage * st::emojiPanSize.height() - int(st::emojiPanSub));
	_emojiAnimations.clear();
	_selected = _pressedSel = -1;
	resize(width(), h);
	_lastMousePos = QCursor::pos();
	updateSelected();
	update();
}

EmojiPan::EmojiPan(QWidget *parent) : TWidget(parent),
_hiding(false), a_opacity(0), _shadow(st::dropdownDef.shadow),
_recent  (this, qsl("emoji_group"), dbietRecent  , QString(), cEmojiTab() == dbietRecent  , st::rbEmojiRecent),
_people  (this, qsl("emoji_group"), dbietPeople  , QString(), cEmojiTab() == dbietPeople  , st::rbEmojiPeople),
_nature  (this, qsl("emoji_group"), dbietNature  , QString(), cEmojiTab() == dbietNature  , st::rbEmojiNature),
_objects (this, qsl("emoji_group"), dbietObjects , QString(), cEmojiTab() == dbietObjects , st::rbEmojiObjects),
_places  (this, qsl("emoji_group"), dbietPlaces  , QString(), cEmojiTab() == dbietPlaces  , st::rbEmojiPlaces),
_symbols (this, qsl("emoji_group"), dbietSymbols , QString(), cEmojiTab() == dbietSymbols , st::rbEmojiSymbols),
_stickers(this, qsl("emoji_group"), dbietStickers, QString(), cEmojiTab() == dbietStickers, st::rbEmojiStickers),
_scroll(this, st::emojiScroll), _inner() {
	setFocusPolicy(Qt::NoFocus);
	_scroll.setFocusPolicy(Qt::NoFocus);
	_scroll.viewport()->setFocusPolicy(Qt::NoFocus);

	if (cEmojiTab() != dbietStickers) {
		_inner.showEmojiPack(cEmojiTab());
	}

	_scroll.setGeometry(st::dropdownDef.padding.left() + st::emojiPanPadding.left(), st::dropdownDef.padding.top() + _recent.height() + st::emojiPanPadding.top(), st::emojiPanPadding.left() + _inner.width() + st::emojiPanPadding.right(), EmojiPadRowsPerPage * st::emojiPanSize.height() - st::emojiPanSub);
	_scroll.setWidget(&_inner);

	_width = st::dropdownDef.padding.left() + st::emojiPanPadding.left() + _scroll.width() + st::emojiPanPadding.right() + st::dropdownDef.padding.right();
	_height = st::dropdownDef.padding.top() + _recent.height() + st::emojiPanPadding.top() + _scroll.height() + st::emojiPanPadding.bottom() + st::dropdownDef.padding.bottom();
	resize(_width, _height);

	int32 left = st::dropdownDef.padding.left() + (_width - st::dropdownDef.padding.left() - st::dropdownDef.padding.right() - 7 * _recent.width()) / 2;
	int32 top = st::dropdownDef.padding.top();
	_recent.move(left, top);  left += _recent.width();
	_people.move(left, top);  left += _people.width();
	_nature.move(left, top);  left += _nature.width();
	_objects.move(left, top); left += _objects.width();
	_places.move(left, top);  left += _places.width();
	_symbols.move(left, top); left += _symbols.width();
	_stickers.move(left, top); left += _stickers.width();

	_hideTimer.setSingleShot(true);
	connect(&_hideTimer, SIGNAL(timeout()), this, SLOT(hideStart()));

	connect(&_recent  , SIGNAL(changed()), this, SLOT(onTabChange()));
	connect(&_people  , SIGNAL(changed()), this, SLOT(onTabChange()));
	connect(&_nature  , SIGNAL(changed()), this, SLOT(onTabChange()));
	connect(&_objects , SIGNAL(changed()), this, SLOT(onTabChange()));
	connect(&_places  , SIGNAL(changed()), this, SLOT(onTabChange()));
	connect(&_symbols , SIGNAL(changed()), this, SLOT(onTabChange()));
	connect(&_stickers, SIGNAL(changed()), this, SLOT(onTabChange()));

	connect(&_scroll, SIGNAL(scrolled()), &_inner, SLOT(updateSelected()));

	connect(&_inner, SIGNAL(emojiSelected(EmojiPtr)), this, SIGNAL(emojiSelected(EmojiPtr)));
	connect(&_inner, SIGNAL(stickerSelected(DocumentData*)), this, SIGNAL(stickerSelected(DocumentData*)));

	if (cPlatform() == dbipMac) {
		connect(App::wnd()->windowHandle(), SIGNAL(activeChanged()), this, SLOT(onWndActiveChanged()));
	}
}

void EmojiPan::onWndActiveChanged() {
	if (!App::wnd()->windowHandle()->isActive() && !isHidden()) {
		leaveEvent(0);
	}
}

void EmojiPan::paintEvent(QPaintEvent *e) {
	QPainter p(this);

	if (!_cache.isNull()) {
		p.setOpacity(a_opacity.current());
	}

	QRect r(st::dropdownDef.padding.left(), st::dropdownDef.padding.top(), _width - st::dropdownDef.padding.left() - st::dropdownDef.padding.right(), _height - st::dropdownDef.padding.top() - st::dropdownDef.padding.bottom());

	// draw shadow
	_shadow.paint(p, r);

	if (_cache.isNull()) {
		p.fillRect(r, st::white->b);
	} else {
		p.drawPixmap(r.left(), r.top(), _cache);
	}
}

void EmojiPan::enterEvent(QEvent *e) {
	_hideTimer.stop();
	if (_hiding) showStart();
}

void EmojiPan::leaveEvent(QEvent *e) {
	if (animating()) {
		hideStart();
	} else {
		_hideTimer.start(300);
	}
}

void EmojiPan::otherEnter() {
	_hideTimer.stop();
	showStart();
}

void EmojiPan::otherLeave() {
	if (animating()) {
		hideStart();
	} else {
		_hideTimer.start(0);
	}
}

void EmojiPan::fastHide() {
	if (animating()) {
		anim::stop(this);
	}
	a_opacity = anim::fvalue(0, 0);
	_hideTimer.stop();
	hide();
	_cache = QPixmap();
}

bool EmojiPan::animStep(float64 ms) {
	float64 dt = ms / st::dropdownDef.duration;
	bool res = true;
	if (dt >= 1) {
		a_opacity.finish();
		if (_hiding) {
			hideFinish();
		} else {
			showAll();
			_cache = QPixmap();
		}
		res = false;
	} else {
		a_opacity.update(dt, anim::linear);
	}
	update();
	return res;
}

void EmojiPan::hideStart() {
	if (_cache.isNull()) {
		showAll();
		_cache = myGrab(this, rect().marginsRemoved(st::dropdownDef.padding));
	}
	hideAll();
	_hiding = true;
	a_opacity.start(0);
	anim::start(this);
}

void EmojiPan::hideFinish() {
	hide();
	_cache = QPixmap();
	_recent.setChecked(true);
}

void EmojiPan::showStart() {
	if (!isHidden() && a_opacity.current() == 1) {
		return;
	}
	if (_cache.isNull()) {
		showAll();
		_cache = myGrab(this, rect().marginsRemoved(st::dropdownDef.padding));
	}
	hideAll();
	_hiding = false;
	show();
	a_opacity.start(1);
	anim::start(this);
	if (_stickers.checked()) emit updateStickers();
}

bool EmojiPan::eventFilter(QObject *obj, QEvent *e) {
	if (e->type() == QEvent::Enter) {
		//if (dynamic_cast<StickerPan*>(obj)) {
		//	enterEvent(e);
		//} else {
			otherEnter();
		//}
	} else if (e->type() == QEvent::Leave) {
		//if (dynamic_cast<StickerPan*>(obj)) {
		//	leaveEvent(e);
		//} else {
			otherLeave();
		//}
	} else if (e->type() == QEvent::MouseButtonPress && static_cast<QMouseEvent*>(e)->button() == Qt::LeftButton/* && !dynamic_cast<StickerPan*>(obj)*/) {
		if (isHidden() || _hiding) {
			otherEnter();
		} else {
			otherLeave();
		}
	}
	return false;
}

void EmojiPan::showAll() {
	_recent.show();
	_people.show();
	_nature.show();
	_objects.show();
	_places.show();
	_symbols.show();
	_stickers.show();
	_scroll.show();
}

void EmojiPan::hideAll() {
	_recent.hide();
	_people.hide();
	_nature.hide();
	_objects.hide();
	_places.hide();
	_symbols.hide();
	_stickers.hide();
	_scroll.hide();
	_inner.clearSelection(true);
}

void EmojiPan::onTabChange() {
	DBIEmojiTab newTab = dbietRecent;
	if (_people.checked()) newTab = dbietPeople;
	else if (_nature.checked()) newTab = dbietNature;
	else if (_objects.checked()) newTab = dbietObjects;
	else if (_places.checked()) newTab = dbietPlaces;
	else if (_symbols.checked()) newTab = dbietSymbols;
	else if (_stickers.checked()) newTab = dbietStickers;
	if (newTab != cEmojiTab()) {
		cSetEmojiTab(newTab);
		Local::writeUserSettings();
		_scroll.scrollToY(0);
	}
	_inner.showEmojiPack(newTab);
	if (newTab == dbietStickers) {
		emit updateStickers();
	}
}

MentionsInner::MentionsInner(MentionsDropdown *parent, MentionRows *rows, HashtagRows *hrows) : _parent(parent), _rows(rows), _hrows(hrows), _sel(-1), _mouseSel(false), _overDelete(false) {
}

void MentionsInner::paintEvent(QPaintEvent *e) {
	QPainter p(this);

	int32 atwidth = st::mentionFont->m.width('@'), hashwidth = st::mentionFont->m.width('#');
	int32 availwidth = width() - 2 * st::mentionPadding.left() - st::mentionPhotoSize - 2 * st::mentionPadding.right();
	int32 htagleft = st::btnAttachPhoto.width + st::taMsgField.textMrg.left() - st::dlgShadow, htagwidth = width() - st::mentionPadding.right() - htagleft;

	int32 from = qFloor(e->rect().top() / st::mentionHeight), to = qFloor(e->rect().bottom() / st::mentionHeight) + 1, last = _rows->isEmpty() ? _hrows->size() : _rows->size();
	for (int32 i = from; i < to; ++i) {
		if (i >= last) break;

		if (i == _sel) {
			p.fillRect(0, i * st::mentionHeight, width(), st::mentionHeight, st::dlgHoverBG->b);
			int skip = (st::mentionHeight - st::notifyClose.icon.pxHeight()) / 2;
			if (_rows->isEmpty()) p.drawPixmap(QPoint(width() - st::notifyClose.icon.pxWidth() - skip, i * st::mentionHeight + skip), App::sprite(), st::notifyClose.icon);
		}
		p.setPen(st::black->p);
		if (_rows->isEmpty()) {
			QString tag = st::mentionFont->m.elidedText('#' + _hrows->at(last - i - 1), Qt::ElideRight, htagwidth);
			p.setFont(st::mentionFont->f);
			p.drawText(htagleft, i * st::mentionHeight + st::mentionTop + st::mentionFont->ascent, tag);
		} else {
			UserData *user = _rows->at(last - i - 1);
			QString first = (_parent->filter().size() < 2) ? QString() : ('@' + user->username.mid(0, _parent->filter().size() - 1)), second = (_parent->filter().size() < 2) ? ('@' + user->username) : user->username.mid(_parent->filter().size() - 1);
			int32 firstwidth = st::mentionFont->m.width(first), secondwidth = st::mentionFont->m.width(second), unamewidth = firstwidth + secondwidth, namewidth = user->nameText.maxWidth();
			if (availwidth < unamewidth + namewidth) {
				namewidth = (availwidth * namewidth) / (namewidth + unamewidth);
				unamewidth = availwidth - namewidth;
				if (firstwidth <= unamewidth) {
					if (firstwidth < unamewidth) {
						first = st::mentionFont->m.elidedText(first, Qt::ElideRight, unamewidth);
					} else if (!second.isEmpty()) {
						first = st::mentionFont->m.elidedText(first + second, Qt::ElideRight, unamewidth);
						second = QString();
					}
				} else {
					second = st::mentionFont->m.elidedText(second, Qt::ElideRight, unamewidth - firstwidth);
				}
			}
			user->photo->load();
			p.drawPixmap(st::mentionPadding.left(), i * st::mentionHeight + st::mentionPadding.top(), user->photo->pix(st::mentionPhotoSize));
			user->nameText.drawElided(p, 2 * st::mentionPadding.left() + st::mentionPhotoSize, i * st::mentionHeight + st::mentionTop, namewidth);
			p.setFont(st::mentionFont->f);

			p.setPen(st::profileOnlineColor->p);
			p.drawText(2 * st::mentionPadding.left() + st::mentionPhotoSize + namewidth + st::mentionPadding.right(), i * st::mentionHeight + st::mentionTop + st::mentionFont->ascent, first);
			if (!second.isEmpty()) {
				p.setPen(st::profileOfflineColor->p);
				p.drawText(2 * st::mentionPadding.left() + st::mentionPhotoSize + namewidth + st::mentionPadding.right() + firstwidth, i * st::mentionHeight + st::mentionTop + st::mentionFont->ascent, second);
			}
		}
	}

	p.fillRect(cWideMode() ? st::dlgShadow : 0, _parent->innerTop(), width() - (cWideMode() ? st::dlgShadow : 0), st::titleShadow, st::titleShadowColor->b);
	p.fillRect(cWideMode() ? st::dlgShadow : 0, _parent->innerBottom() - st::titleShadow, width() - (cWideMode() ? st::dlgShadow : 0), st::titleShadow, st::titleShadowColor->b);
}

void MentionsInner::mouseMoveEvent(QMouseEvent *e) {
	_mousePos = mapToGlobal(e->pos());
	_mouseSel = true;
	onUpdateSelected(true);
}

void MentionsInner::clearSel() {
	_mouseSel = _overDelete = false;
	setSel(-1);
}

bool MentionsInner::moveSel(int direction) {
	_mouseSel = false;
	int32 maxSel = (_rows->isEmpty() ? _hrows->size() : _rows->size());
	if (_sel >= maxSel || _sel < 0) {
		if (direction < 0) setSel(maxSel - 1, true);
		return (_sel >= 0 && _sel < maxSel);
	}
	if (_sel > 0 || direction > 0) {
		setSel((_sel + direction >= maxSel) ? -1 : (_sel + direction), true);
	}
	return true;
}

bool MentionsInner::select() {
	int32 maxSel = (_rows->isEmpty() ? _hrows->size() : _rows->size());
	if (_sel >= 0 && _sel < maxSel) {
		QString result = _rows->isEmpty() ? ('#' + _hrows->at(_hrows->size() - _sel - 1)) : ('@' + _rows->at(_rows->size() - _sel - 1)->username);
		emit chosen(result);
		return true;
	}
	return false;
}

void MentionsInner::mousePressEvent(QMouseEvent *e) {
	_mousePos = mapToGlobal(e->pos());
	_mouseSel = true;
	onUpdateSelected(true);
	if (e->button() == Qt::LeftButton) {
		if (_overDelete && _sel >= 0 && _sel < _hrows->size()) {
			_mousePos = mapToGlobal(e->pos());

			QString toRemove = _hrows->at(_hrows->size() - _sel - 1);
			RecentHashtagPack recent(cRecentWriteHashtags());
			for (RecentHashtagPack::iterator i = recent.begin(); i != recent.cend();) {
				if (i->first == toRemove) {
					i = recent.erase(i);
				} else {
					++i;
				}
			}
			cSetRecentWriteHashtags(recent);
			Local::writeRecentHashtags();
			_parent->updateFiltered();

			_mouseSel = true;
			onUpdateSelected(true);
		} else {
			select();
		}
	}
}

void MentionsInner::enterEvent(QEvent *e) {
	setMouseTracking(true);
	_mousePos = QCursor::pos();
	onUpdateSelected(true);
}

void MentionsInner::leaveEvent(QEvent *e) {
	setMouseTracking(false);
	if (_sel >= 0) {
		setSel(-1);
	}
}

void MentionsInner::setSel(int sel, bool scroll) {
	_sel = sel;
	parentWidget()->update();
	int32 maxSel = _rows->isEmpty() ? _hrows->size() : _rows->size();
	if (scroll && _sel >= 0 && _sel < maxSel) emit mustScrollTo(_sel * st::mentionHeight, (_sel + 1) * st::mentionHeight);
}

void MentionsInner::onUpdateSelected(bool force) {
	QPoint mouse(mapFromGlobal(_mousePos));
	if ((!force && !rect().contains(mouse)) || !_mouseSel) return;

	int w = width(), mouseY = mouse.y();
	_overDelete = _rows->isEmpty() && (mouse.x() >= w - st::mentionHeight);
	int32 sel = mouseY / int32(st::mentionHeight), maxSel = _rows->isEmpty() ? _hrows->size() : _rows->size();
	if (sel < 0 || sel >= maxSel) {
		sel = -1;
	}
	if (sel != _sel) {
		setSel(sel);
	}
}

void MentionsInner::onParentGeometryChanged() {
	_mousePos = QCursor::pos();
	if (rect().contains(mapFromGlobal(_mousePos))) {
		setMouseTracking(true);
		onUpdateSelected(true);
	}
}

MentionsDropdown::MentionsDropdown(QWidget *parent) : QWidget(parent),
_scroll(this, st::mentionScroll), _inner(this, &_rows, &_hrows), _chat(0), _hiding(false), a_opacity(0), _shadow(st::dropdownDef.shadow) {
	_hideTimer.setSingleShot(true);
	connect(&_hideTimer, SIGNAL(timeout()), this, SLOT(hideStart()));
	connect(&_inner, SIGNAL(chosen(QString)), this, SIGNAL(chosen(QString)));
	connect(&_inner, SIGNAL(mustScrollTo(int,int)), &_scroll, SLOT(scrollToY(int,int)));

	setFocusPolicy(Qt::NoFocus);
	_scroll.setFocusPolicy(Qt::NoFocus);
	_scroll.viewport()->setFocusPolicy(Qt::NoFocus);

	_inner.setGeometry(rect());
	_scroll.setGeometry(rect());

	_scroll.setWidget(&_inner);
	_scroll.show();
	_inner.show();

	connect(&_scroll, SIGNAL(geometryChanged()), &_inner, SLOT(onParentGeometryChanged()));
	connect(&_scroll, SIGNAL(scrolled()), &_inner, SLOT(onUpdateSelected()));

	if (cPlatform() == dbipMac) {
		connect(App::wnd()->windowHandle(), SIGNAL(activeChanged()), this, SLOT(onWndActiveChanged()));
	}
}

void MentionsDropdown::paintEvent(QPaintEvent *e) {
	QPainter p(this);

	if (animating()) {
		p.setOpacity(a_opacity.current());
		p.drawPixmap(0, 0, _cache);
		return;
	}

	p.fillRect(rect(), st::white->b);

}

void MentionsDropdown::showFiltered(ChatData *chat, QString start) {
	_chat = chat;
	start = start.toLower();
	bool toDown = (_filter != start);
	if (toDown) {
		_filter = start;
	}

	updateFiltered(toDown);
}

void MentionsDropdown::updateFiltered(bool toDown) {
	int32 now = unixtime();
	QMultiMap<int32, UserData*> ordered;
	MentionRows rows;
	HashtagRows hrows;
	if (_filter.at(0) == '@') {
		rows.reserve(_chat->participants.isEmpty() ? _chat->lastAuthors.size() : _chat->participants.size());
		if (_chat->participants.isEmpty()) {
			if (_chat->count > 0) {
				App::api()->requestFullPeer(_chat);
			}
		} else {
			for (ChatData::Participants::const_iterator i = _chat->participants.cbegin(), e = _chat->participants.cend(); i != e; ++i) {
				UserData *user = i.key();
				if (user->username.isEmpty()) continue;
				if (_filter.size() > 1 && (!user->username.startsWith(_filter.midRef(1), Qt::CaseInsensitive) || user->username.size() + 1 == _filter.size())) continue;
				ordered.insertMulti(App::onlineForSort(user->onlineTill, now), user);
			}
		}
		for (MentionRows::const_iterator i = _chat->lastAuthors.cbegin(), e = _chat->lastAuthors.cend(); i != e; ++i) {
			UserData *user = *i;
			if (user->username.isEmpty()) continue;
			if (_filter.size() > 1 && (!user->username.startsWith(_filter.midRef(1), Qt::CaseInsensitive) || user->username.size() + 1 == _filter.size())) continue;
			rows.push_back(user);
			if (!ordered.isEmpty()) {
				ordered.remove(App::onlineForSort(user->onlineTill, now), user);
			}
		}
		if (!ordered.isEmpty()) {
			for (QMultiMap<int32, UserData*>::const_iterator i = ordered.cend(), b = ordered.cbegin(); i != b;) {
				--i;
				rows.push_back(i.value());
			}
		}
	} else {
		const RecentHashtagPack &recent(cRecentWriteHashtags());
		hrows.reserve(recent.size());
		for (RecentHashtagPack::const_iterator i = recent.cbegin(), e = recent.cend(); i != e; ++i) {
			if (_filter.size() > 1 && (!i->first.startsWith(_filter.midRef(1), Qt::CaseInsensitive) || i->first.size() + 1 == _filter.size())) continue;
			hrows.push_back(i->first);
		}
	}
	if (rows.isEmpty() && hrows.isEmpty()) {
		if (!isHidden()) {
			hideStart();
			_rows.clear();
			_hrows.clear();
		}
	} else {
		_rows = rows;
		_hrows = hrows;
		bool hidden = _hiding || isHidden();
		if (hidden) {
			show();
			_scroll.show();
		}
		recount(toDown);
		if (hidden) {
			hide();
			showStart();
		}
	}
}

void MentionsDropdown::setBoundings(QRect boundings) {
	_boundings = boundings;
	resize(_boundings.width(), height());
	_scroll.resize(size());
	_inner.resize(width(), _inner.height());
	recount();
}

void MentionsDropdown::recount(bool toDown) {
	int32 h = (_rows.isEmpty() ? _hrows.size() : _rows.size()) * st::mentionHeight, oldst = _scroll.scrollTop(), st = oldst;
	
	if (_inner.height() != h) {
		st += h - _inner.height();
		_inner.resize(width(), h);
	}
	if (h > _boundings.height()) h = _boundings.height();
	if (h > 5 * st::mentionHeight) h = 5 * st::mentionHeight;
	if (height() != h) {
		st += _scroll.height() - h;
		setGeometry(0, _boundings.height() - h, width(), h);
		_scroll.resize(width(), h);
	} else if (y() != _boundings.height() - h) {
		move(0, _boundings.height() - h);
	}
	if (toDown) st = _scroll.scrollTopMax();
	if (st != oldst) _scroll.scrollToY(st);
	if (toDown) _inner.clearSel();
}

void MentionsDropdown::fastHide() {
	if (animating()) {
		anim::stop(this);
	}
	a_opacity = anim::fvalue(0, 0);
	_hideTimer.stop();
	hideFinish();
}

void MentionsDropdown::hideStart() {
	if (!_hiding) {
		if (_cache.isNull()) {
			_scroll.show();
			_cache = myGrab(this, rect());
		}
		_scroll.hide();
		_hiding = true;
		a_opacity.start(0);
		anim::start(this);
	}
}

void MentionsDropdown::hideFinish() {
	hide();
	_hiding = false;
	_filter = qsl("-");
	_inner.clearSel();
}

void MentionsDropdown::showStart() {
	if (!isHidden() && a_opacity.current() == 1 && !_hiding) {
		return;
	}
	if (_cache.isNull()) {
		_scroll.show();
		_cache = myGrab(this, rect());
	}
	_scroll.hide();
	_hiding = false;
	show();
	a_opacity.start(1);
	anim::start(this);
}

bool MentionsDropdown::animStep(float64 ms) {
	float64 dt = ms / st::dropdownDef.duration;
	bool res = true;
	if (dt >= 1) {
		a_opacity.finish();
		_cache = QPixmap();
		if (_hiding) {
			hideFinish();
		} else {
			_scroll.show();
			_inner.clearSel();
		}
		res = false;
	} else {
		a_opacity.update(dt, anim::linear);
	}
	update();
	return res;
}

const QString &MentionsDropdown::filter() const {
	return _filter;
}

int32 MentionsDropdown::innerTop() {
	return _scroll.scrollTop();
}

int32 MentionsDropdown::innerBottom() {
	return _scroll.scrollTop() + _scroll.height();
}

bool MentionsDropdown::eventFilter(QObject *obj, QEvent *e) {
	if (isHidden()) return QWidget::eventFilter(obj, e);
	if (e->type() == QEvent::KeyPress) {
		QKeyEvent *ev = static_cast<QKeyEvent*>(e);
		if (ev->key() == Qt::Key_Up) {
			_inner.moveSel(-1);
			return true;
		} else if (ev->key() == Qt::Key_Down) {
			return _inner.moveSel(1);
		} else if (ev->key() == Qt::Key_Enter || ev->key() == Qt::Key_Return || ev->key() == Qt::Key_Space) {
			return _inner.select();
		}
	}
	return QWidget::eventFilter(obj, e);
}

MentionsDropdown::~MentionsDropdown() {
}
